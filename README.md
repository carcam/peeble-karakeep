# Karakeep Reader for Pebble

Lee en tu Pebble los artículos guardados en tu instancia de [Karakeep](https://karakeep.app).

- Lista de marcadores (sin archivar / favoritos / todos), configurable con Clay.
- Lector con scroll; el texto se extrae del contenido ya rastreado por Karakeep.
- Pulsación larga en Select (lista) = refrescar.

## Uso
1. `pebble build && pebble install --emulator emery` (o `--phone <ip>`).
2. En la app del móvil, abre los ajustes: URL de tu Karakeep y API key (Ajustes > API Keys).

## Arquitectura
`src/pkjs/index.js` habla con `/api/v1` de Karakeep (Bearer) y envía al reloj la lista y el
texto del artículo en trozos de 300 caracteres con ACK; `src/c/main.c` pinta lista y lector.

Plataformas: emery, gabbro, flint, diorite. Licencia MIT.
