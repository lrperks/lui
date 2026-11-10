# Lui

[![CI](https://github.com/lrperks/lui/actions/workflows/ci.yml/badge.svg)](https://github.com/lrperks/lui/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

Dear ImGui, written as markup. A `.lui` file is plain Lua plus XML-style tags:
every tag is one ImGui widget, attributes set its values, and `<on>` blocks
handle its events.

Lui keeps ImGui's speed and widget set, and adds a cleaner default theme,
retained state, data binding, and background HTTP requests.

![The weather example](docs/weather.png)

```xml
<lui title="Counter" width=360 height=160>
local clicks = 0

<button>
    "Clicked {clicks} times"
    <on click>
        clicks = clicks + 1
    </on>
</button>

</lui>
```

## Install

```bash
git clone --recursive https://github.com/lrperks/lui.git
cd lui
./install.sh
```

`install.sh` installs the build dependencies with your package manager (apt,
dnf or pacman), builds raylib if it isn't installed, and puts a single `lui`
command in `~/.local/bin`. Use `--prefix /usr/local` to install elsewhere, or
`--no-deps` if you have the dependencies already.

Lui is developed and tested on Linux with X11. It needs a C/C++ compiler,
Lua 5.3 or 5.4, [raylib](https://www.raylib.com/) 5.5+, and `curl` (for
`L.get`). Dear ImGui and rlImGui come with the repository as submodules. The
DejaVu fonts are used when installed; set `$LUI_FONT_DIR` to point elsewhere.

To build by hand instead: `make` (pass `RAYLIB_PREFIX=…` if raylib isn't in
`~/.local`), then `make install PREFIX=~/.local`.

## Usage

```bash
lui new hello          # create hello.lui from a starter template
lui watch hello.lui    # open it; every save reloads the window
lui run app.lui        # open an app (also: lui app.lui)
lui build app.lui      # compile to app.lui.lua without opening a window
```

`lui watch` reports compile errors as `file:line: message` and waits for the
next save. Lua files in a `plugins/` folder next to the app are loaded first
(see [Custom elements](#custom-elements)).

`LUI_SHOT=out.png lui run app.lui` saves a screenshot after 120 frames
(`LUI_SHOT_FRAMES` to change) and exits — handy for docs and CI.

## Markup

- Everything outside a tag is Lua. `local name = <tag>…</tag>` binds an element
  to a Lua local, so handlers can change it (`name.text = "…"`).
- Elements are `<name> children </name>` or self-closing `<name />`. Names are
  case-sensitive.
- Attributes go inside the opening tag:
  - `key=12` or `key=1.5`: a number
  - `key="text"`: a string
  - `key={lua-expr}`: a Lua expression, re-evaluated every frame
  - `key`: a bare name means `true`
- A quoted string inside a tag is its text. `"Hi {name}"` interpolates Lua.
- `<on EVENT> … </on>` runs Lua when the event fires. The handler receives the
  element as `self`.
- `<!-- … -->` is a comment.

## Elements

| Tag | ImGui | Attributes | Events |
|---|---|---|---|
| `text` / `label` | Text | `wrap` | |
| `button` | Button | `width`, `height`, `bg` | `click` |
| `input` | InputText | `label`, `placeholder`, `width`, `height` (multi-line) | `change`, `submit` |
| `checkbox` | Checkbox | `label`, `checked` | `change` |
| `radio` | RadioButton | `label`, `checked` | `click` |
| `slider` | SliderFloat / SliderInt | `label`, `value`, `min`, `max`, `format`, `int` | `change` |
| `drag` | DragFloat | `label`, `value`, `speed`, `min`, `max`, `format` | `change` |
| `combo` | Combo | `label`, `items`, `selected` | `change` |
| `color` | ColorEdit3 | `label`, `value` | `change` |
| `progress` | ProgressBar | `value` (0–1), `text` overlay | |
| `plot` | PlotLines / PlotHistogram | `label`, `values`, `width`, `height`, `histogram` | |
| `selectable` | Selectable | `label`, `selected` | `click` |
| `separator` | Separator / SeparatorText | `text` | |
| `spacer` | Dummy | `width`, `height` | |
| `row` | SameLine between children | `gap` | |
| `column` / `group` | BeginGroup | | |
| `panel` | BeginChild, fits its content | `width`, `height`, `bg`, `border` | |
| `scroll` | BeginChild, scrolls | `height` (default 200), `bg`, `border` | |
| `window` | Begin | `title`, `width`, `height`, `x`, `y` | |
| `tabs` / `tab` | TabBar / TabItem | `label` (on `tab`) | |
| `tree` | TreeNode | `label`, `open` | |
| `collapse` | CollapsingHeader | `label`, `open` | |
| `menubar` / `menu` / `item` | MenuBar / Menu / MenuItem | `label` (`menu`, `item`), `shortcut`, `checked` | `click` (`item`) |
| `table` / `tr` / `td` | Table | `columns` (`"A,B"` or a count), `border` | |
| `canvas` | draw list | `width`, `height`, `bg`, `radius` | `draw`, `click` |

Every element also takes:

- `color`: text colour
- `size` and `weight` (0 regular, 1 bold, 2 light): font
- `tooltip`: text shown on hover
- `hover`, `leave`: fire when the pointer enters or leaves the element
- `frame`: runs every frame. Its second argument is the frame time in seconds.

Colours can be `"#rgb"`, `"#rrggbb"`, a name (`red`, `green`, `blue`, `gray`,
`yellow`, `orange`, `white`, `black`), or a number `0xRRGGBB`.

### Window

The `<lui>` tag configures the app: `title`, `width`, `height`,
`theme` (`dark` by default, or `light`), `accent`, `bg`, and `resizable`
(`true` by default). Top-level elements that are not `<window>`s fill the
screen. `<window>`s float on top.

## Drawing

A `<canvas>` `draw` handler gets `g`:

- `g.rect(x, y, w, h, radius, color, alpha)`
- `g.outline(x, y, w, h, radius, color, thickness)`
- `g.gradient(x, y, w, h, top, bottom)`
- `g.line(x1, y1, x2, y2, thickness, color)`
- `g.circle(x, y, r, color, alpha)`
- `g.text(s, x, y, size, color, weight)`
- `g.measure(s, size, weight)` returns the width of `s`
- `g.width`, `g.height`

Coordinates are relative to the canvas.

## Data and background requests

- `L.get(url, callback)` fetches a URL without blocking the window. It calls
  `callback(body)` on success or `callback(nil, message)` on failure.
- `L.json(text)` decodes JSON (`null` becomes `nil`). It returns `nil, message`
  on bad input.
- `L.urlencode(s)` escapes a query value.

## Custom elements

Plugins add elements with `L.define`. An element is drawn on its own canvas:

```lua
L.define("badge", {
    measure = function(n) return L.measure(n.text, 14, 1) + 18, 24 end,
    draw = function(n, g) g.rect(0, 0, g.width, g.height, g.height / 2, L.color(n.bg, 0x3b82f6)) end,
})
```

See `examples/plugins/badge.lua` for a complete example, used by
`examples/badges.lui`.

## How it works

- `src/luic.c` compiles `.lui` into a Lua chunk that builds the element tree
  and registers the handlers. `bin/luic` is the same compiler on its own.
- `src/lui.lua` is the runtime. It walks the element tree every frame and
  calls ImGui through the `ig` table. It is built into the `lui` binary
  (`src/runtime.S`); `LUI_RUNTIME=src/lui.lua` loads it from disk instead.
- `src/lui_imgui.cpp` binds the `ig` table to Dear ImGui and sets the theme.
- `src/main.c` is the `lui` command: the raylib window, the frame loop,
  background HTTP (curl on worker threads), and `run`/`watch`/`build`/`new`.

## Examples

- `examples/widgets.lui`: every element
- `examples/weather.lui`: background requests and a live layout (needs network)
- `examples/demo.lui`, `examples/counter.lui`, `examples/canvas.lui`,
  `examples/badges.lui`: smaller samples

## Tests

```bash
make test
```

compiles every example and compares the output with `tests/expected/`, checks
it is valid Lua, and checks that the files in `tests/errors/` are rejected with
a line number. After an intended change to the compiler's output, regenerate
the expected files with the command in `tests/run.sh`.

CI (`.github/workflows/ci.yml`) also builds raylib, runs the tests, renders
every example under a virtual display (the screenshots are uploaded as a build
artifact), and checks `install.sh`.

`tools/test_click.py` opens `examples/counter.lui` in a real window, clicks
the button through XTest, and checks the handler ran twice. It needs
`xwininfo`, `libX11` and `libXtst`.

```bash
python3 tools/test_click.py examples/counter.lui
```

## License

[MIT](LICENSE)
