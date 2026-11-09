local L=require"lui"
L.cfg={}
L.cfg.title="Plugins"
L.cfg.width=360
L.cfg.height=160
L.cfg.theme="dark"
local count = 3
local card=L.node("panel")
L.root(card)
card.gap=12
local _w=L.node("row")
L.add(card,_w)
_w.gap=8
local _e=L.node("badge")
L.add(_w,_e)
_e.bg="#22c55e"
_e.color="#0b1020"
_e.text="New"
local _r=L.node("badge")
L.add(_w,_r)
_r.text="Beta"
local _t=L.node("label")
L.add(card,_t)
_t.text="Badges come from plugins/badge.lua"
local _y=L.node("button")
L.add(card,_y)
_y.text="Clicks"
L.on(_y,"click",function(self,g,dt)count = count + 1
end)
