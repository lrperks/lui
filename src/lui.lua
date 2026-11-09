-- lui runtime: the element tree, drawn every frame with Dear ImGui.
--
-- Each tag is an ImGui widget (see the table `widgets` below).  Generated code
-- (from luic) builds the tree with L.node / L.root / L.add / L.on / L.bind /
-- L.bindtext; the host calls L.run once and L.frame every frame.

local host, ig = lui_host, ig
local L = {}

local roots, all = {}, {}
local debug = os.getenv("LUI_DEBUG")

-- ── errors ──────────────────────────────────────────────────────────
-- A failing handler or binding doesn't stop the app: it is logged once and
-- listed at the bottom of the window.
local errors = {}

local function report(err)
    local msg = tostring(err)
    if msg == errors[#errors] then return end
    errors[#errors + 1] = msg
    if #errors > 3 then table.remove(errors, 1) end
    io.stderr:write("lui: ", msg, "\n")
end

local function safe(f, ...)
    local ok, err = pcall(f, ...)
    if not ok then report(err) end
    return ok
end

-- ── colours ─────────────────────────────────────────────────────────

local named = {
    white = 0xffffff, black = 0x000000, red = 0xef4444, green = 0x22c55e,
    blue = 0x3b82f6, gray = 0x6b7280, yellow = 0xeab308, orange = 0xf97316,
}

-- "#rgb", "#rrggbb", a name, or a number -> 0xRRGGBB (nil if none)
local function color(c)
    if type(c) == "number" then return c end
    if type(c) ~= "string" then return nil end
    if named[c] then return named[c] end
    local hex = c:match("^#(%x+)$")
    if hex and #hex == 3 then hex = hex:gsub(".", "%0%0") end
    return hex and #hex == 6 and tonumber(hex, 16) or nil
end

function L.color(c, dflt) return color(c) or dflt end

-- ── building ────────────────────────────────────────────────────────

function L.node(tag)
    local n = { tag = tag, kids = {}, on = {}, live = {}, text = "" }
    all[#all + 1] = n
    n.uid = #all
    return n
end

function L.root(n) roots[#roots + 1] = n end
function L.add(parent, child) parent.kids[#parent.kids + 1] = child end
function L.on(n, ev, f) n.on[ev] = f end
function L.bind(n, key, f) n.live[key] = f end
function L.bindtext(n, f) n.live.text = f end

local function fire(n, ev)
    local f = n.on[ev]
    if f then
        safe(f, n)
        if debug and ev == "click" then io.stderr:write("click ", n.text, "\n") end
    end
end

-- ── drawing on a canvas ─────────────────────────────────────────────
-- Handed to <canvas> draw handlers and custom elements.  Coordinates are
-- relative to the canvas; colours are anything color() accepts.

local function measure(s, size, weight)
    return ig.measure(tostring(s), size or 17, weight)
end

local function painter(x0, y0, w, h)
    return {
        width = w, height = h,
        rect = function(x, y, rw, rh, r, c, a) ig.draw_rect(x0 + x, y0 + y, rw, rh, r or 0, color(c) or 0, a) end,
        outline = function(x, y, rw, rh, r, c, t) ig.draw_outline(x0 + x, y0 + y, rw, rh, r or 0, color(c) or 0, t) end,
        gradient = function(x, y, rw, rh, top, bottom)
            ig.draw_gradient(x0 + x, y0 + y, rw, rh, color(top) or 0, color(bottom) or 0)
        end,
        line = function(x1, y1, x2, y2, t, c) ig.draw_line(x0 + x1, y0 + y1, x0 + x2, y0 + y2, t or 1, color(c) or 0) end,
        circle = function(x, y, r, c, a) ig.draw_circle(x0 + x, y0 + y, r, color(c) or 0, a) end,
        text = function(s, x, y, size, c, weight)
            ig.draw_text(tostring(s), x0 + x, y0 + y, size or 17, color(c) or 0xffffff, weight)
        end,
        measure = measure,
    }
end

-- ── widgets ─────────────────────────────────────────────────────────

local render

local function kids(n)
    for _, k in ipairs(n.kids) do render(k) end
end

-- ImGui label: visible text plus a hidden id, so equal labels don't clash.
local function label(n)
    return (n.label or n.text or "") .. "##" .. n.uid
end

local function hidden(n) return "##" .. n.uid end

local function width(n, fill)
    if n.width then ig.set_width(n.width) elseif fill then ig.set_width() end
end

local widgets = {}

widgets.window = function(n)
    local menubar = false
    for _, k in ipairs(n.kids) do menubar = menubar or k.tag == "menubar" end
    if ig.begin_window((n.title or n.text) .. "##" .. n.uid, n.width, n.height, menubar, n.x, n.y) then kids(n) end
    ig.end_window()
end

widgets.text = function(n) ig.text(n.text, n.wrap) end
widgets.label = widgets.text

widgets.button = function(n)
    local pushed = n.bg and ig.push_color("button", color(n.bg))
    if ig.button(label(n), n.width, n.height) then fire(n, "click") end
    if pushed then ig.pop_color(pushed) end
end

widgets.input = function(n)
    width(n, true)
    local text, changed, entered = ig.input(n.label and label(n) or hidden(n), n.text, n.placeholder, n.height)
    if changed then n.text = text; fire(n, "change") end
    if entered then fire(n, "submit") end
end

widgets.checkbox = function(n)
    local v, changed = ig.checkbox(label(n), n.checked)
    if changed then n.checked = v; fire(n, "change") end
end

widgets.radio = function(n)
    if ig.radio(label(n), n.checked) then fire(n, "click") end
end

widgets.slider = function(n)
    width(n)
    local v, changed = ig.slider(label(n), n.value or n.min or 0, n.min or 0, n.max or 1, n.format, n.int)
    if changed then n.value = v; fire(n, "change") end
end

widgets.drag = function(n)
    width(n)
    local v, changed = ig.drag(label(n), n.value or 0, n.speed, n.min, n.max, n.format)
    if changed then n.value = v; fire(n, "change") end
end

widgets.combo = function(n)
    width(n)
    local i, changed = ig.combo(label(n), n.selected or 1, n.items or {})
    if changed then n.selected = i; fire(n, "change") end
end

widgets.color = function(n)
    width(n)
    local c, changed = ig.color(label(n), color(n.value) or 0xffffff)
    if changed then n.value = c; fire(n, "change") end
end

widgets.progress = function(n)
    ig.progress(n.value or 0, n.width, n.height, n.text ~= "" and n.text or nil)
end

widgets.plot = function(n)
    ig.plot(label(n), n.values or {}, n.width, n.height, n.histogram)
end

widgets.selectable = function(n)
    if ig.selectable(label(n), n.selected) then fire(n, "click") end
end

widgets.separator = function(n) ig.separator(n.text) end
widgets.spacer = function(n) ig.spacing(n.width or 0, n.height or 0) end

widgets.row = function(n)
    for i, k in ipairs(n.kids) do
        if i > 1 then ig.same_line(n.gap) end
        render(k)
    end
end

widgets.column = function(n)
    ig.begin_group()
    kids(n)
    ig.end_group()
end
widgets.group = widgets.column

-- A bordered card; height 0 (the default) fits the content.
widgets.panel = function(n)
    ig.begin_child("panel" .. n.uid, n.width or 0, n.height or 0, n.border ~= false, color(n.bg))
    kids(n)
    ig.end_child()
end

-- A scrolling region of fixed height.
widgets.scroll = function(n)
    ig.begin_child("scroll" .. n.uid, n.width or 0, n.height or 200, n.border == true, color(n.bg))
    kids(n)
    ig.end_child()
end

widgets.tabs = function(n)
    if ig.begin_tab_bar("tabs" .. n.uid) then
        kids(n)
        ig.end_tab_bar()
    end
end

widgets.tab = function(n)
    if ig.begin_tab(label(n)) then
        kids(n)
        ig.end_tab()
    end
end

widgets.tree = function(n)
    if ig.tree(label(n), n.open) then
        kids(n)
        ig.tree_pop()
    end
end

widgets.collapse = function(n)
    if ig.collapsing(label(n), n.open) then kids(n) end
end

widgets.menubar = function(n)
    if ig.begin_menu_bar() then
        kids(n)
        ig.end_menu_bar()
    end
end

widgets.menu = function(n)
    if ig.begin_menu(label(n)) then
        kids(n)
        ig.end_menu()
    end
end

widgets.item = function(n)
    if ig.menu_item(label(n), n.shortcut, n.checked) then fire(n, "click") end
end

-- <table columns="Name,Size"> holding <tr> rows of <td> cells.
widgets.table = function(n)
    local heads, count = {}, tonumber(n.columns)
    if not count then
        for h in tostring(n.columns or ""):gmatch("[^,]+") do heads[#heads + 1] = h end
        count = math.max(#heads, 1)
    end
    if ig.begin_table("table" .. n.uid, count, n.border) then
        if #heads > 0 then
            for _, h in ipairs(heads) do ig.table_column(h) end
            ig.table_headers()
        end
        kids(n)
        ig.end_table()
    end
end

widgets.tr = function(n)
    ig.table_row()
    for _, cell in ipairs(n.kids) do
        ig.table_cell()
        render(cell)
    end
end

widgets.td = function(n)
    if n.text ~= "" then ig.text(n.text, n.wrap) end
    kids(n)
end

widgets.canvas = function(n)
    local x, y, w, h, hovered, clicked = ig.canvas("canvas" .. n.uid, n.width, n.height or 100)
    n.w, n.h = w, h
    if clicked then fire(n, "click") end
    if n.bg then ig.draw_rect(x, y, w, h, n.radius or 0, color(n.bg)) end
    if n.on.draw then safe(n.on.draw, n, painter(x, y, w, h)) end
end

-- ── custom elements ─────────────────────────────────────────────────
-- L.define(tag, { measure = function(n) return w, h end, draw = function(n, g) end })
-- adds an element drawn on its own canvas.  Events, binds and attributes work
-- as on built-in elements.

function L.define(tag, spec)
    widgets[tag] = function(n)
        local w, h = 100, 40
        if spec.measure then w, h = spec.measure(n) end
        local x, y, cw, ch, _, clicked = ig.canvas("custom" .. n.uid, w, h)
        if clicked then fire(n, "click") end
        if spec.draw then safe(spec.draw, n, painter(x, y, cw, ch)) end
    end
end

L.measure = measure

-- ── rendering ───────────────────────────────────────────────────────

local unknown = {}

function render(n)
    for k, f in pairs(n.live) do
        local ok, v = pcall(f)
        if ok then n[k] = v else report(v) end
    end
    local draw = widgets[n.tag]
    if not draw then
        if not unknown[n.tag] then report("unknown element <" .. n.tag .. ">") end
        unknown[n.tag] = true
        return
    end

    ig.push_id(n.uid)
    local font = n.size or n.weight
    if font then ig.push_font(n.weight or 0, n.size or 0) end
    local colors = n.color and ig.push_color("text", color(n.color))
    safe(draw, n)
    if colors then ig.pop_color(colors) end
    if font then ig.pop_font() end

    if n.tooltip then ig.tooltip(n.tooltip) end
    if n.on.hover or n.on.leave then
        local now = ig.hovered()
        if now ~= (n.hovering or false) then
            n.hovering = now
            fire(n, now and "hover" or "leave")
        end
    end
    ig.pop_id()
end

-- ── background requests ─────────────────────────────────────────────
-- L.get(url, callback) fetches without blocking the window.
-- callback(body) on success, or callback(nil, message) on failure.

local requests = {}

function L.get(url, callback)
    local id = host.http_start(url)
    if not id then return callback(nil, "too many requests in flight") end
    requests[#requests + 1] = { id = id, callback = callback }
end

local function poll()
    local i = 1
    while i <= #requests do
        local r = requests[i]
        local state, body = host.http_poll(r.id)
        if state == "pending" then
            i = i + 1
        else
            table.remove(requests, i)
            if state == "ok" then safe(r.callback, body) else safe(r.callback, nil, body) end
        end
    end
end

function L.urlencode(s)
    return (s:gsub("[^%w%-%._~]", function(c) return string.format("%%%02X", c:byte()) end))
end

-- JSON -> Lua tables (null -> nil).  Returns nil, message on bad input.
function L.json(text)
    local pos = 1
    local value
    local function ws() pos = text:find("[^ \t\r\n]", pos) or #text + 1 end
    local function str()
        local out, i = {}, pos + 1
        while true do
            local c = text:sub(i, i)
            if c == "" then error("unterminated string") end
            if c == '"' then break end
            if c == "\\" then
                local e = text:sub(i + 1, i + 1)
                if e == "u" then
                    out[#out + 1] = utf8.char(tonumber(text:sub(i + 2, i + 5), 16))
                    i = i + 6
                else
                    out[#out + 1] = ({ n = "\n", t = "\t", r = "\r", b = "\b", f = "\f" })[e] or e
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
    local function list(close, item)
        pos = pos + 1
        ws()
        if text:sub(pos, pos) == close then pos = pos + 1 return end
        while true do
            item()
            ws()
            local d = text:sub(pos, pos)
            pos = pos + 1
            if d == close then return end
            assert(d == ",", "expected ',' or '" .. close .. "'")
        end
    end
    function value()
        ws()
        local c = text:sub(pos, pos)
        if c == "{" then
            local obj = {}
            list("}", function()
                ws()
                local k = str()
                ws()
                assert(text:sub(pos, pos) == ":", "expected ':'")
                pos = pos + 1
                obj[k] = value()
            end)
            return obj
        elseif c == "[" then
            local arr, count = {}, 0
            list("]", function() count = count + 1; arr[count] = value() end)
            return arr
        elseif c == '"' then
            return str()
        end
        if text:sub(pos, pos + 3) == "true" then pos = pos + 4 return true end
        if text:sub(pos, pos + 4) == "false" then pos = pos + 5 return false end
        if text:sub(pos, pos + 3) == "null" then pos = pos + 4 return nil end
        local num = text:match("^-?%d+%.?%d*[eE]?[-+]?%d*", pos)
        assert(num and #num > 0, "unexpected character at " .. pos)
        pos = pos + #num
        return tonumber(num)
    end
    local ok, res = pcall(value)
    if not ok then return nil, res end
    return res
end

-- ── frame ───────────────────────────────────────────────────────────

-- Opens the window from the <lui> attributes: title, width, height,
-- theme ("dark" default, or "light"), accent, bg, resizable.
function L.run()
    local c = L.cfg or {}
    host.open(c.title or "Lui", c.width or 640, c.height or 480, c.resizable ~= false)
    ig.theme(c.theme ~= "light", color(c.accent), color(c.bg))
end

-- One frame.  Roots that aren't <window>s fill the screen; windows float on top.
function L.frame()
    if host.should_close() then return false end
    host.begin(0)
    poll()
    local dt = host.dt()
    for _, n in ipairs(all) do
        if n.on.frame then safe(n.on.frame, n, nil, dt) end
    end

    local menubar = false
    for _, r in ipairs(roots) do menubar = menubar or r.tag == "menubar" end
    ig.begin_main(menubar)
    for _, r in ipairs(roots) do
        if r.tag ~= "window" then render(r) end
    end
    if #errors > 0 then
        ig.separator()
        local pushed = ig.push_color("text", 0xf87171)
        for _, msg in ipairs(errors) do ig.text(msg, true) end
        ig.pop_color(pushed)
    end
    ig.end_window()
    for _, r in ipairs(roots) do
        if r.tag == "window" then render(r) end
    end
    host.finish()
    return true
end

return L
