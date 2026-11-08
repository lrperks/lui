-- lui runtime: element tree, themes, layout, input, drawing.
--
-- Generated code (from luic) calls L.node / L.root / L.add / L.on / L.bind /
-- L.bindtext and sets L.cfg from the <lui> attributes.  The host (lui_host)
-- provides the `host` table backed by raylib.

local host = lui_host
local L = {}

local roots, all = {}, {}
local hot, active = nil, nil
local debug = os.getenv("LUI_DEBUG")

-- ── themes ──────────────────────────────────────────────────────────

local themes = {
    light = {
        bg = 0xf3f4f6, surface = 0xffffff, text = 0x111827, muted = 0x9ca3af,
        border = 0xd1d5db, accent = 0x3b82f6, accentText = 0xffffff,
    },
    dark = {
        bg = 0x0f1115, surface = 0x1c1f26, text = 0xe5e7eb, muted = 0x6b7280,
        border = 0x2d323c, accent = 0x60a5fa, accentText = 0x0b1020,
    },
}

-- Active theme: a preset plus any accent / bg overrides from <lui>.
local theme = themes.light

local named = {
    white = 0xffffff, black = 0x000000, red = 0xe53935, green = 0x43a047,
    blue = 0x3b82f6, gray = 0x6b7280, none = -1,
}

local function color(c, dflt)
    if c == nil then return dflt end
    if type(c) == "number" then return c end
    if named[c] then return named[c] end
    local hex = c:match("^#(%x+)$")
    if hex and #hex == 3 then
        hex = hex:gsub(".", function(d) return d .. d end)
    end
    if hex and #hex == 6 then return tonumber(hex, 16) end
    return dflt
end

local function shade(c, d)
    local function clamp(v) return math.max(0, math.min(255, v)) end
    local r = clamp(((c >> 16) & 255) + d)
    local g = clamp(((c >> 8) & 255) + d)
    local b = clamp((c & 255) + d)
    return (r << 16) | (g << 8) | b
end

-- ── building ────────────────────────────────────────────────────────

-- Per-tag defaults.  Colours are left nil here: the theme is applied after
-- the tree is built (L.run), so draw() resolves them from the active theme.
local function defaults(tag)
    if tag == "button" then
        return { pad = 10, radius = 6, weight = 1, size = 18 }
    elseif tag == "input" then
        return { pad = 8, radius = 6, size = 18, width = 200, placeholder = "" }
    elseif tag == "checkbox" then
        return { size = 18, gap = 8, pad = 0 }
    elseif tag == "panel" then
        return { pad = 16, radius = 10, gap = 12 }
    elseif tag == "label" then
        return { size = 18 }
    elseif tag == "scroll" then
        return { gap = 8, size = 18, scroll = 0, content = 0 }
    elseif tag == "canvas" then
        return { width = 200, height = 150 }
    end
    return { gap = 8, size = 18 }
end

function L.node(tag)
    local n = {
        tag = tag, kids = {}, on = {}, live = {}, text = "",
        bg = nil, fg = nil, border = nil, pad = 0, gap = 8, size = 18, weight = 0,
        radius = 0, align = "start", width = 0, height = 0,
        x = 0, y = 0, w = 0, h = 0, hover = false, down = false, checked = false,
        scroll = 0, content = 0,
    }
    for k, v in pairs(defaults(tag)) do n[k] = v end
    all[#all + 1] = n
    return n
end

function L.root(n) roots[#roots + 1] = n end
function L.add(parent, child) parent.kids[#parent.kids + 1] = child end
function L.on(n, ev, f) n.on[ev] = f end
function L.bind(n, key, f) n.live[key] = f end
function L.bindtext(n, f) n.live.text = f end

-- ── network and data ───────────────────────────────────────────────
--
-- L.fetch(url) runs curl and returns the body, or nil and an error message.
-- It blocks until the request finishes, so keep requests short and rare.

function L.urlencode(str)
    return (str:gsub("[^%w%-%._~]", function(c)
        return string.format("%%%02X", c:byte())
    end))
end

function L.fetch(url)
    local cmd = "curl -sfL --max-time 10 '" .. url:gsub("'", "%%27") .. "' 2>/dev/null"
    local p = io.popen(cmd)
    if not p then return nil, "could not start curl" end
    local body = p:read("a")
    local ok = p:close()
    if not ok or body == "" then return nil, "request failed: " .. url end
    return body
end

-- Minimal JSON decoder: objects -> tables, arrays -> tables, numbers, strings,
-- booleans, null -> nil.  Returns nil and a message on malformed input.
function L.json(text)
    local pos = 1
    local function ws() pos = text:find("[^ \t\r\n]", pos) or #text + 1 end
    local value
    local function str()
        local out, i = {}, pos + 1
        while true do
            local c = text:sub(i, i)
            if c == "" then error("unterminated string") end
            if c == '"' then break end
            if c == "\\" then
                local e = text:sub(i + 1, i + 1)
                local map = { n = "\n", t = "\t", r = "\r", b = "\b", f = "\f" }
                if e == "u" then
                    out[#out + 1] = utf8.char(tonumber(text:sub(i + 2, i + 5), 16))
                    i = i + 6
                else
                    out[#out + 1] = map[e] or e
                    i = i + 2
                end
            else
                out[#out + 1] = c
                i = i + 1
            end
        end
        pos = i + 1
        return table.concat(out)
    end
    function value()
        ws()
        local c = text:sub(pos, pos)
        if c == "{" then
            local obj = {}
            pos = pos + 1
            ws()
            if text:sub(pos, pos) == "}" then pos = pos + 1 return obj end
            while true do
                ws()
                local k = str()
                ws()
                assert(text:sub(pos, pos) == ":", "expected ':'")
                pos = pos + 1
                obj[k] = value()
                ws()
                local d = text:sub(pos, pos)
                pos = pos + 1
                if d == "}" then return obj end
                assert(d == ",", "expected ',' or '}'")
            end
        elseif c == "[" then
            local arr = {}
            pos = pos + 1
            ws()
            if text:sub(pos, pos) == "]" then pos = pos + 1 return arr end
            while true do
                arr[#arr + 1] = value()
                ws()
                local d = text:sub(pos, pos)
                pos = pos + 1
                if d == "]" then return arr end
                assert(d == ",", "expected ',' or ']'")
            end
        elseif c == '"' then
            return str()
        elseif text:sub(pos, pos + 3) == "true" then pos = pos + 4 return true
        elseif text:sub(pos, pos + 4) == "false" then pos = pos + 5 return false
        elseif text:sub(pos, pos + 3) == "null" then pos = pos + 4 return nil
        end
        local num = text:match("^-?%d+%.?%d*[eE]?[-+]?%d*", pos)
        assert(num and #num > 0, "unexpected character at " .. pos)
        pos = pos + #num
        return tonumber(num)
    end
    local ok, res = pcall(value)
    if not ok then return nil, res end
    return res
end

-- ── extension API ───────────────────────────────────────────────────
--
-- L.define(tag, spec) adds a new element type.  spec fields (all optional):
--   measure(n) -> w, h          size the element (children are already measured)
--   draw(n, g)                  paint it; g is the canvas painter (see below)
--   container = true            also draw and lay out child elements
-- Events, binds and attributes work on custom elements exactly as on built-in ones.
local custom = {}

function L.define(tag, spec) custom[tag] = spec end
function L.measure(s, size, weight) return host.measure(s, size or 18, weight or 0) end
function L.color(c, dflt) return color(c, dflt) end

-- ── layout ──────────────────────────────────────────────────────────

local BOX = 18  -- checkbox square

local function measure(n)
    for _, k in ipairs(n.kids) do measure(k) end
    local def = custom[n.tag]
    if def and def.measure then
        n.w, n.h = def.measure(n)
        n.content = n.h
        return
    end
    local cw, ch = 0, 0
    if n.tag == "label" or n.tag == "button" then
        cw = host.measure(n.text, n.size, n.weight)
        ch = n.size + 4
    elseif n.tag == "input" then
        cw = n.width
        ch = n.size + 4
    elseif n.tag == "checkbox" then
        cw = BOX + n.gap + host.measure(n.text, n.size, n.weight)
        ch = math.max(BOX, n.size + 4)
    elseif n.tag == "canvas" then
        cw, ch = n.width, n.height
    elseif n.tag == "row" then
        for _, k in ipairs(n.kids) do
            cw = cw + k.w
            if k.h > ch then ch = k.h end
        end
        if #n.kids > 1 then cw = cw + n.gap * (#n.kids - 1) end
    else -- column, panel, root containers
        for _, k in ipairs(n.kids) do
            ch = ch + k.h
            if k.w > cw then cw = k.w end
        end
        if #n.kids > 1 then ch = ch + n.gap * (#n.kids - 1) end
    end
    n.content = ch
    local padded = n.tag ~= "checkbox"
    n.w = cw + (padded and n.pad * 2 or 0)
    n.h = ch + (padded and n.pad * 2 or 0)
    if n.width > 0 and n.tag ~= "input" then n.w = n.width end
    if n.fill then n.w = n.fill end   -- roots follow the window width
    if n.height > 0 then n.h = n.height end
end

local function place(n, x, y)
    n.x, n.y = x, y
    local cx, cy = x + n.pad, y + n.pad - n.scroll  -- scroll is 0 except in <scroll>
    local iw, ih = n.w - n.pad * 2, n.h - n.pad * 2
    for _, k in ipairs(n.kids) do
        if n.tag == "row" then
            if n.align == "stretch" then k.h = ih end
            local dy = n.align == "center" and (ih - k.h) // 2 or 0
            place(k, cx, cy + dy)
            cx = cx + k.w + n.gap
        else
            if n.align == "stretch" then k.w = iw end
            local dx = n.align == "center" and (iw - k.w) // 2 or 0
            place(k, cx + dx, cy)
            cy = cy + k.h + n.gap
        end
    end
end

-- ── input ───────────────────────────────────────────────────────────

local function hit(n, mx, my)
    if mx < n.x or my < n.y or mx >= n.x + n.w or my >= n.y + n.h then
        return nil
    end
    local found = n
    for _, k in ipairs(n.kids) do
        found = hit(k, mx, my) or found
    end
    return found
end

local function fire(n, ev)
    local f = n.on[ev]
    if f then
        f(n)
        if debug and ev == "click" then
            io.stderr:write(ev, " ", n.text, "\n")
        end
    end
end

local function toggle(n)
    n.checked = not n.checked
    fire(n, "change")
end

-- The deepest <scroll> under the pointer, or nil.
local function scrollAt(n, mx, my)
    if mx < n.x or my < n.y or mx >= n.x + n.w or my >= n.y + n.h then return nil end
    local found = n.tag == "scroll" and n or nil
    for _, k in ipairs(n.kids) do found = scrollAt(k, mx, my) or found end
    return found
end

local function wheel(root, mx, my)
    local delta = host.wheel()
    if delta == 0 then return end
    local s = scrollAt(root, mx, my)
    if not s then return end
    local max = math.max(0, s.content - (s.h - s.pad * 2))
    s.scroll = math.max(0, math.min(max, s.scroll - delta * 24))
end

local function update(root)
    local mx, my, _, pressed, released = host.mouse()
    wheel(root, mx, my)
    local now = hit(root, mx, my)
    if now ~= hot then
        if hot then hot.hover = false; fire(hot, "leave") end
        hot = now
        if now then now.hover = true; fire(now, "hover") end
    end
    if hot and pressed then
        hot.down = true
        active = hot
        fire(hot, "press")
    end
    if active and released then
        active.down = false
        fire(active, "release")
        if active == hot then
            if active.tag == "checkbox" then toggle(active) end
            fire(active, "click")
        end
        active = nil
    end
end

-- ── drawing ─────────────────────────────────────────────────────────

local function fill(n, c)
    host.rect(n.x, n.y, n.w, n.h, n.radius, c)
end

local function frame_border(n, c, thick)
    host.outline(n.x, n.y, n.w, n.h, n.radius, c, thick or 1)
end

-- Drawing API handed to a canvas's `draw` handler.  Coordinates are relative
-- to the canvas's top-left corner.
local function painter(n)
    local ox, oy = n.x, n.y
    return {
        rect = function(x, y, w, h, r, c)
            host.rect(ox + x, oy + y, w, h, r or 0, color(c, theme.text))
        end,
        outline = function(x, y, w, h, r, c, t)
            host.outline(ox + x, oy + y, w, h, r or 0, color(c, theme.border), t or 1)
        end,
        line = function(x1, y1, x2, y2, t, c)
            host.line(ox + x1, oy + y1, ox + x2, oy + y2, t or 1, color(c, theme.text))
        end,
        circle = function(x, y, r, c)
            host.circle(ox + x, oy + y, r, color(c, theme.text))
        end,
        text = function(s, x, y, size, c)
            host.text(s, ox + x, oy + y, size or 18, 0, color(c, theme.text))
        end,
        width = n.w,
        height = n.h,
    }
end

local function draw(n)
    for k, f in pairs(n.live) do n[k] = f() end
    local def = custom[n.tag]
    if def then
        if def.draw then def.draw(n, painter(n)) end
        if def.container then
            for _, k in ipairs(n.kids) do draw(k) end
        end
        return
    end

    if n.tag == "button" then
        local c = color(n.bg, theme.accent)
        if n.down then c = shade(c, -35) elseif n.hover then c = shade(c, 22) end
        fill(n, c)
        local tw = host.measure(n.text, n.size, n.weight)
        host.text(n.text, n.x + (n.w - tw) // 2, n.y + (n.h - n.size) // 2 - 2,
                  n.size, n.weight, color(n.fg, theme.accentText))

    elseif n.tag == "input" then
        -- Native-quality editing (selection, clipboard, undo) comes from Dear ImGui.
        local text, entered, changed = host.textfield(
            tostring(n.uid), n.x, n.y, n.w, n.h, n.text, n.placeholder, n.size)
        if changed then
            n.text = text
            fire(n, "change")
        end
        if entered then fire(n, "submit") end

    elseif n.tag == "checkbox" then
        local bx, by = n.x, n.y + (n.h - BOX) // 2
        host.outline(bx, by, BOX, BOX, 3, color(n.border, theme.border), 2)
        if n.checked then
            host.rect(bx + 4, by + 4, BOX - 8, BOX - 8, 2, color(n.bg, theme.accent))
        end
        host.text(n.text, bx + BOX + n.gap, n.y + (n.h - n.size) // 2 - 2,
                  n.size, n.weight, color(n.fg, theme.text))

    elseif n.tag == "canvas" then
        if n.bg then fill(n, color(n.bg, nil)) end
        if n.on.draw then
            host.clip(n.x, n.y, n.w, n.h)
            n.on.draw(n, painter(n))
            host.unclip()
        end

    else -- label, row, column, panel, scroll
        if n.tag == "panel" then
            fill(n, color(n.bg, theme.surface))
            frame_border(n, color(n.border, theme.border))
        elseif n.bg then
            fill(n, color(n.bg, nil))
        end
        if n.text ~= "" then
            host.text(n.text, n.x + n.pad, n.y + n.pad, n.size, n.weight, color(n.fg, theme.text))
        end
        if n.tag == "scroll" then
            host.clip(n.x, n.y, n.w, n.h)
            for _, k in ipairs(n.kids) do draw(k) end
            host.unclip()
            return
        end
    end

    for _, k in ipairs(n.kids) do draw(k) end
end

-- ── frame loop ──────────────────────────────────────────────────────

-- One frame: input -> layout -> paint.  Returns false once the window closes.
function L.frame()
    if host.should_close() then return false end
    host.begin(theme.bg)
    local dt = host.dt()
    for _, n in ipairs(all) do
        if n.on.frame then n.on.frame(n, nil, dt) end
    end
    -- Roots stretch to the window width, so the layout follows every resize.
    local sw, sh = host.size()
    for _, r in ipairs(roots) do
        r.fill = math.max(0, sw - 40)
    end
    local y = 20
    for _, r in ipairs(roots) do
        measure(r)
        place(r, 20, y)
        update(r)
        draw(r)
        y = y + r.h + 20
    end
    host.finish()
    return true
end

-- Applies <lui theme="..." accent="..." bg="..."> and opens the window.
function L.run()
    local c = L.cfg or {}
    theme = themes[c.theme or "light"] or themes.light
    theme = setmetatable({
        accent = color(c.accent, theme.accent),
        bg = color(c.bg, theme.bg),
    }, { __index = theme })
    host.open(c.title or "Lui", c.width or 480, c.height or 300, c.resizable ~= false)
end

return L
