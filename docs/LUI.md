# Lui markup (custom XML + Lua)

A `.lui` file is one custom XML document, not HTML. Tags are parsed uniformly.
Lua runs wherever the markup needs logic, so a file reads as one language.

## Example

```
<lui>
let clicks = 0

let button = <button>
    "Click me"
    <on click>
        clicks = clicks + 1
        button.text = "Clicks: " .. clicks
    </on>
</button>

</lui>
```

## Rules

- **Declarations.** `let name = <tag> ... </tag>` binds an element to a Lua
  local. Plain `let name = expr` is ordinary Lua. There are no ids and no
  `id=` attributes: a name comes from `let`.
- **Elements.** `<name> children </name>`, or self-closing `<name />`.
  Names are case-sensitive.
- **Attributes.** Written inside the opening tag, after the name:
  - `key=123` number, `key=1.5` number
  - `key="text"` string
  - `key={lua-expr}` Lua expression, re-evaluated each frame
  - bare `key` means `true`
- **Text.** A quoted string is a text node. `{lua-expr}` inside it is
  interpolated, e.g. `"Clicks: {clicks}"`.
- **Lua statements.** Any line that is not markup is Lua, including `let`,
  `if`, `for`, and function definitions.
- **Events.** `<on EVENT> lua-body </on>` attaches a handler to the enclosing
  element. Events: `click`, `press`, `release`, `hover`, `leave`. Inside the
  body, the enclosing element is referred to by its `let` name.
- **Comments.** `<!-- ... -->`.

## Runtime model

`luic` (Komodo) parses the markup into a Lua chunk that builds the element tree
and registers the handlers. `lui_host` (C) embeds Lua 5.3, owns the raylib
window, and draws each frame. Handlers and `{expr}` interpolations run in that
Lua VM.
