#include <pebble.h>

#define MAX_ITEMS 30
#define TITLE_LEN 84
#define ARTICLE_MAX 16384

static Window *s_list_window;
static MenuLayer *s_menu;
static char s_titles[MAX_ITEMS][TITLE_LEN];
static int s_count = 0;
static char s_status[96] = "Cargando...";

static Window *s_reader_window;
static ScrollLayer *s_scroll;
static TextLayer *s_text;
static char s_article[ARTICLE_MAX];
static bool s_reader_open = false;

static void send_request(uint32_t key, int32_t value) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) == APP_MSG_OK) {
    dict_write_int32(out, key, value);
    app_message_outbox_send();
  }
}

/* ---------- Lector ---------- */

static void reader_relayout(void) {
  Layer *root = scroll_layer_get_layer(s_scroll);
  GRect bounds = layer_get_bounds(root);
  text_layer_set_text(s_text, s_article);
  GSize size = text_layer_get_content_size(s_text);
  size.h += 20;
  layer_set_frame(text_layer_get_layer(s_text), GRect(0, 0, bounds.size.w, size.h));
  scroll_layer_set_content_size(s_scroll, GSize(bounds.size.w, size.h));
}

static void reader_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  s_scroll = scroll_layer_create(bounds);
  scroll_layer_set_click_config_onto_window(s_scroll, window);
  s_text = text_layer_create(GRect(PBL_IF_ROUND_ELSE(12, 4), 0,
                                   bounds.size.w - PBL_IF_ROUND_ELSE(24, 8), 2000));
  text_layer_set_font(s_text, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  scroll_layer_add_child(s_scroll, text_layer_get_layer(s_text));
  layer_add_child(root, scroll_layer_get_layer(s_scroll));
  strcpy(s_article, "Cargando...");
  reader_relayout();
  s_reader_open = true;
}

static void reader_unload(Window *window) {
  s_reader_open = false;
  text_layer_destroy(s_text);
  scroll_layer_destroy(s_scroll);
}

static void open_reader(int index) {
  if (!s_reader_window) {
    s_reader_window = window_create();
    window_set_window_handlers(s_reader_window, (WindowHandlers) {
      .load = reader_load, .unload = reader_unload,
    });
  }
  window_stack_push(s_reader_window, true);
  send_request(MESSAGE_KEY_REQ_ARTICLE, index);
}

/* ---------- Lista ---------- */

static uint16_t menu_rows(MenuLayer *m, uint16_t section, void *ctx) {
  return s_count > 0 ? s_count : 1;
}

static int16_t menu_cell_height(MenuLayer *m, MenuIndex *idx, void *ctx) {
  return PBL_IF_ROUND_ELSE(menu_layer_is_index_selected(m, idx) ? 80 : 50, 52);
}

static void menu_draw_row(GContext *ctx, const Layer *cell, MenuIndex *idx, void *data) {
  GRect b = layer_get_bounds(cell);
  graphics_context_set_text_color(ctx, GColorBlack);
  const char *txt = s_count > 0 ? s_titles[idx->row] : s_status;
  graphics_draw_text(ctx, txt, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(6, 2, b.size.w - 12, b.size.h - 4),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void menu_select(MenuLayer *m, MenuIndex *idx, void *ctx) {
  if (s_count > 0) { open_reader(idx->row); }
}

static void menu_select_long(MenuLayer *m, MenuIndex *idx, void *ctx) {
  s_count = 0;
  strcpy(s_status, "Actualizando...");
  menu_layer_reload_data(s_menu);
  send_request(MESSAGE_KEY_REQ_LIST, 1);
}

static void list_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_menu = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks) {
    .get_num_rows = menu_rows,
    .get_cell_height = menu_cell_height,
    .draw_row = menu_draw_row,
    .select_click = menu_select,
    .select_long_click = menu_select_long,
  });
  menu_layer_set_click_config_onto_window(s_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_menu));
}

static void list_unload(Window *window) {
  menu_layer_destroy(s_menu);
}

/* ---------- AppMessage ---------- */

static void inbox_received(DictionaryIterator *it, void *ctx) {
  Tuple *t;

  if ((t = dict_find(it, MESSAGE_KEY_ERROR))) {
    if (s_reader_open) {
      snprintf(s_article, sizeof(s_article), "%s", t->value->cstring);
      reader_relayout();
    } else {
      s_count = 0;
      snprintf(s_status, sizeof(s_status), "%s", t->value->cstring);
      menu_layer_reload_data(s_menu);
    }
    return;
  }

  if ((t = dict_find(it, MESSAGE_KEY_ITEM_COUNT)) && t->value->int32 == 0) {
    s_count = 0;
    strcpy(s_status, "No hay artículos");
    menu_layer_reload_data(s_menu);
    return;
  }

  Tuple *idx = dict_find(it, MESSAGE_KEY_ITEM_INDEX);
  Tuple *title = dict_find(it, MESSAGE_KEY_ITEM_TITLE);
  if (idx && title) {
    int i = idx->value->int32;
    if (i >= 0 && i < MAX_ITEMS) {
      snprintf(s_titles[i], TITLE_LEN, "%s", title->value->cstring);
      if (i == 0) { s_count = 0; }
      if (i + 1 > s_count) { s_count = i + 1; }
      menu_layer_reload_data(s_menu);
    }
    return;
  }

  Tuple *aidx = dict_find(it, MESSAGE_KEY_ART_INDEX);
  Tuple *atext = dict_find(it, MESSAGE_KEY_ART_TEXT);
  if (aidx && atext && s_reader_open) {
    size_t used = aidx->value->int32 == 0 ? 0 : strlen(s_article);
    snprintf(s_article + used, sizeof(s_article) - used, "%s", atext->value->cstring);
    reader_relayout();
  }
}

static void inbox_dropped(AppMessageResult reason, void *ctx) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "inbox dropped: %d", (int) reason);
}

/* ---------- Ciclo de vida ---------- */

static void init(void) {
  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_open(1200, 64);

  s_list_window = window_create();
  window_set_window_handlers(s_list_window, (WindowHandlers) {
    .load = list_load, .unload = list_unload,
  });
  window_stack_push(s_list_window, true);
}

static void deinit(void) {
  if (s_reader_window) { window_destroy(s_reader_window); }
  window_destroy(s_list_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
