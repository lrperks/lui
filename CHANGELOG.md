# Changelog

## Unreleased

### Added
- Dear ImGui runtime: every tag is an ImGui widget — text, button, input,
  checkbox, radio, slider, drag, combo, color, progress, plot, selectable,
  separator, spacer, row, column, panel, scroll, window, tabs, tree, collapse,
  menubar/menu/item, table/tr/td, and canvas.
- Softer default theme (dark or light) with one accent colour.
- `L.get` background HTTP requests that don't block the window.
- In-window error list: a failing handler no longer stops the app.
- Plugins: `L.define` adds custom elements drawn on their own canvas.
- `LUI_SHOT` screenshot mode, `make test` compiler tests, and CI.
- Examples: `widgets` (every element) and a redesigned `weather` app.

### Changed
- `.lui` files are plain Lua plus tags: `local name = <tag>` replaces `let`.
- The compiler, `luic`, is written in C.
- Dear ImGui and rlImGui build from source as git submodules.
- Fonts are found in common Linux and macOS font folders or `$LUI_FONT_DIR`,
  falling back to ImGui's built-in font.
