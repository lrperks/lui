# Lui

A GUI runtime where markup and logic live in one file. Write elements
(`<panel>`, `<button>`, `<input>`, …) and Lua together; Lui builds the window
on raylib.

```xml
<lui title="Counter" width=420 height=200 theme="dark" accent="#22c55e">
let clicks = 0

let button = <button>
    "Clicked {clicks} times"
    <on click>
        clicks = clicks + 1
    </on>
</button>

</lui>
```

## Use

```bash
./lui run counter.lui      # compile and open
./lui watch counter.lui    # reopen the window on every save
./lui build counter.lui    # compile only -> counter.lui.lua
```

## Elements

| Tag | Purpose | Attributes | Events |
|---|---|---|---|
| `column` | stacks children vertically (default container) | `gap`, `pad`, `align` | |
| `row` | stacks children horizontally | `gap`, `pad`, `align` | |
| `panel` | card with surface colour, border, rounded corners | `bg`, `border`, `gap`, `pad`, `radius` | |
| `label` | text; `"{expr}"` updates each frame | `size`, `weight`, `color` | |
| `button` | clickable; hover and press shading | `bg`, `color`, `size`, `radius`, `weight` | `click`, `press`, `release`, `hover`, `leave` |
| `input` | single-line text field | `placeholder`, `width`, `text` | `change`, `submit` |
| `checkbox` | box with a label | `checked`, `bg` (fill colour) | `change`, `click` |

Every element also takes `width` and `height`. `align` is `start`, `center`,
or `stretch`. Colours are `#rgb`, `#rrggbb`, or a name (`white`, `red`, …).

## Window (`<lui>`)

| Attribute | Default |
|---|---|
| `title` | `Lui` |
| `width`, `height` | `480`, `300` |
| `theme` | `light` (or `dark`) |
| `accent` | the theme's accent |
| `bg` | the theme's background |
| `resizable` | `true`; set `false` to lock the size |

## Events

Each `<on EVENT>` body is Lua. Inside it, `self` is the element that fired.
Use the element's `let` name when you need to reach it from elsewhere.

## Files

- `luic.km` — the compiler (Komodo): `.lui` → Lua
- `lui.lua` — the runtime: tree, layout, input, themes, drawing
- `lui_host.c` — the raylib host that runs the Lua
- `lui` — the command-line entry point
- `test_click.py` — clicks the counter demo and checks its log

## Extending Lui

Any `plugins/*.lua` beside a `.lui` file is loaded before the app. A plugin
adds element types with `lui.define`:

```lua
local L = require("lui")

L.define("badge", {
    measure = function(n)                  -- size the element: return w, h
        return L.measure(n.text, n.size, 1) + 16, n.size + 8
    end,
    draw = function(n, g)                  -- paint it; g is relative to the element
        g.rect(0, 0, n.w, n.h, n.h // 2, L.color(n.bg, 0x3b82f6))
        g.text(n.text, 8, 4, n.size, L.color(n.fg, 0xffffff))
    end,
})
```

Then `<badge bg="#22c55e">"New"</badge>` works in any `.lui` file. A spec may
set `container = true` to also lay out and draw child elements. Events, binds,
and attributes work on custom elements the same way as on built-in ones.

Other hooks: `<canvas>` with `<on draw>` for free drawing, `<on frame>` for
per-frame logic (`dt` is the frame time), and `<scroll>` for clipped, wheel-
scrollable content.

## Data and network

- `L.fetch(url)` runs `curl` and returns the body, or `nil` and a message. It
  blocks until the request finishes.
- `L.json(text)` decodes JSON into Lua tables, or returns `nil` and a message.
- `L.urlencode(s)` escapes a query value.

`weather.lui` is a complete app built on these: it looks up a city with
Open-Meteo (no API key), shows the current temperature and conditions, and
draws the hourly temperatures on a canvas.

```bash
./lui run weather.lui
```

## Resizing

Windows are resizable by default (minimum 240×160). Root elements stretch to
the window width and re-lay out on every resize, so content, scroll views,
and canvases follow the window.
