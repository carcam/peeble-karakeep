# Karakeep Reader for Pebble

Read the articles saved in your own [Karakeep](https://karakeep.app) instance on your Pebble.

This is an unofficial community app and is not affiliated with Karakeep.

**Install it from the [Pebble app store](https://apps.repebble.com/a76ab108742840bfa7cc869a).**

- Bookmark list (not archived / favourites / all), configurable with Clay.
- Scrolling reader; the text comes from the content Karakeep has already crawled.
- Long-press Select in the list to refresh.
- Text size follows the watch setting by default, or choose one for the list and one for the article.
- Light and dark themes.

## Usage
1. `pebble build && pebble install --emulator emery` (or `--phone <ip>`).
2. In the phone app, open the app settings and enter your Karakeep URL and API key (Settings > API Keys in Karakeep).

## Architecture
`src/pkjs/index.js` talks to Karakeep's `/api/v1` (Bearer auth) and sends the list and the
article text to the watch in 300-character chunks, waiting for an ACK after each one;
`src/c/main.c` draws the list and the reader.

Platforms: emery, gabbro, flint, diorite.

## License

MIT, see [LICENSE](LICENSE).

Project page: https://carcam.github.io/peeble-karakeep/
