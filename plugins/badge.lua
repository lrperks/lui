-- Example plugin: a <badge> element, a small rounded pill with text.
--   <badge bg="#22c55e" color="#0b1020">"New"</badge>
local L = require("lui")

L.define("badge", {
    measure = function(n)
        local tw = L.measure(n.text, n.size, 1)
        return tw + 16, n.size + 8
    end,
    draw = function(n, g)
        g.rect(0, 0, n.w, n.h, n.h // 2, L.color(n.bg, 0x3b82f6))
        g.text(n.text, 8, 4, n.size, L.color(n.fg, 0xffffff))
    end,
})
