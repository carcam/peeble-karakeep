var Clay = require('@rebble/clay');
var clay = new Clay(require('./config.json'), null, { autoHandleEvents: true });

var CHUNK_CHARS = 300;     // caracteres por mensaje (<= 900 bytes en UTF-8)
var MAX_ARTICLE_CHARS = 14000;
var items = [];            // [{id, title}] de la última lista enviada

function settings() {
  var s = {};
  try { s = JSON.parse(localStorage.getItem('clay-settings') || '{}'); } catch (e) {}
  var url = (s.SERVER_URL || '').trim().replace(/\/+$/, '').replace(/\/api(\/v1)?$/, '');
  return {
    url: url,
    key: (s.API_KEY || '').trim(),
    filter: s.LIST_FILTER || 'unarchived',
    max: parseInt(s.MAX_ITEMS, 10) || 20
  };
}

function api(path, cb) {
  var cfg = settings();
  if (!cfg.url || !cfg.key) { return cb(new Error('Configura la URL y la API key en el móvil')); }
  var req = new XMLHttpRequest();
  req.open('GET', cfg.url + '/api/v1' + path, true);
  req.setRequestHeader('Authorization', 'Bearer ' + cfg.key);
  req.timeout = 20000;
  req.onload = function () {
    if (req.status === 401) { return cb(new Error('API key no válida')); }
    if (req.status !== 200) { return cb(new Error('Error del servidor ' + req.status)); }
    try { cb(null, JSON.parse(req.responseText)); } catch (e) { cb(new Error('Respuesta no válida')); }
  };
  req.onerror = req.ontimeout = function () { cb(new Error('No se pudo conectar')); };
  req.send();
}

// Envía los mensajes de uno en uno esperando el ACK del reloj, con reintentos.
function sendQueue(msgs, i, tries) {
  if (i >= msgs.length) { return; }
  Pebble.sendAppMessage(msgs[i], function () {
    sendQueue(msgs, i + 1, 0);
  }, function () {
    if (tries < 3) { sendQueue(msgs, i, tries + 1); }
  });
}

function sendError(err) {
  Pebble.sendAppMessage({ ERROR: String(err.message || err).substring(0, 80) });
}

function bookmarkTitle(b) {
  var c = b.content || {};
  return (b.title || c.title || c.url || (c.text || '').split('\n')[0] || 'Sin título')
    .replace(/\s+/g, ' ').trim().substring(0, 78);
}

function sendList() {
  var cfg = settings();
  var q = '/bookmarks?limit=' + cfg.max + '&includeContent=false';
  if (cfg.filter === 'unarchived') { q += '&archived=false'; }
  if (cfg.filter === 'favourites') { q += '&favourited=true'; }
  api(q, function (err, data) {
    if (err) { return sendError(err); }
    var list = (data.bookmarks || []).slice(0, cfg.max);
    items = list.map(function (b) { return { id: b.id, title: bookmarkTitle(b) }; });
    if (!items.length) { return Pebble.sendAppMessage({ ITEM_COUNT: 0 }); }
    var msgs = items.map(function (it, n) {
      var m = { ITEM_INDEX: n, ITEM_TITLE: it.title };
      if (n === 0) { m.ITEM_COUNT = items.length; }
      return m;
    });
    sendQueue(msgs, 0, 0);
  });
}

var ENTITIES = { amp: '&', lt: '<', gt: '>', quot: '"', apos: "'", nbsp: ' ',
  hellip: '...', mdash: '-', ndash: '-', lsquo: "'", rsquo: "'", ldquo: '"', rdquo: '"' };

function htmlToText(html) {
  return html
    .replace(/<(script|style|noscript|svg)[\s\S]*?<\/\1>/gi, '')
    .replace(/<\/(p|div|h[1-6]|li|blockquote|tr|pre)>|<br\s*\/?>/gi, '\n\n')
    .replace(/<li[^>]*>/gi, '- ')
    .replace(/<[^>]+>/g, '')
    .replace(/&#(\d+);/g, function (_, n) { return String.fromCharCode(n); })
    .replace(/&#x([0-9a-f]+);/gi, function (_, n) { return String.fromCharCode(parseInt(n, 16)); })
    .replace(/&([a-z]+);/gi, function (m, n) { return ENTITIES[n.toLowerCase()] || m; })
    .replace(/[ \t]+/g, ' ')
    .replace(/ ?\n ?/g, '\n')
    .replace(/\n{3,}/g, '\n\n')
    .trim();
}

function sendArticle(index) {
  var it = items[index];
  if (!it) { return sendError('Lista caducada, vuelve a abrirla'); }
  api('/bookmarks/' + encodeURIComponent(it.id) + '?includeContent=true', function (err, b) {
    if (err) { return sendError(err); }
    var c = b.content || {};
    var text = c.htmlContent ? htmlToText(c.htmlContent) : (c.text || c.description || '');
    if (!text) { text = 'Este marcador no tiene contenido guardado.\n\n' + (c.url || ''); }
    if (text.indexOf(it.title) !== 0) { text = it.title + '\n\n' + text; }
    text = text.substring(0, MAX_ARTICLE_CHARS);
    var total = Math.ceil(text.length / CHUNK_CHARS);
    var msgs = [];
    for (var n = 0; n < total; n++) {
      msgs.push({ ART_INDEX: n, ART_TOTAL: total,
                  ART_TEXT: text.substr(n * CHUNK_CHARS, CHUNK_CHARS) });
    }
    sendQueue(msgs, 0, 0);
  });
}

Pebble.addEventListener('ready', sendList);
Pebble.addEventListener('appmessage', function (e) {
  var p = e.payload;
  if (p.REQ_LIST !== undefined) { sendList(); }
  if (p.REQ_ARTICLE !== undefined) { sendArticle(p.REQ_ARTICLE); }
});
