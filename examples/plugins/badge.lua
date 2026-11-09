-- Example plugin: a <badge> element, a small rounded pill with text.
--   <badge bg="#22c55e" color="#0b1020">"New"</badge>
local L = require("lui")

L.define("badge", {
    measure = function(n)
        local size = n.size or 14
        return L.measure(n.text, size, 1) + 18, size + 10
    end,
    draw = function(n, g)
        local size = n.size or 14
        g.rect(0, 0, g.width, g.height, g.height / 2, L.color(n.bg, 0x3b82f6))
        g.text(n.text, 9, 5, size, L.color(n.color, 0xffffff), 1)
    end,
})
