local L=require"lui"
L.cfg={}
L.cfg.title="Lui demo"
L.cfg.width=420
L.cfg.height=420
L.cfg.theme="dark"
L.cfg.accent="#22c55e"
local clicks = 0
local name = ""
local card=L.node("panel")
L.root(card)
card.gap=14
local heading=L.node("label")
L.add(card,heading)
heading.size=24
heading.weight=1
L.bindtext(heading,function()return "Hello, "..tostring(name).."!" end)
local _w=L.node("input")
L.add(card,_w)
_w.placeholder="Your name"
_w.width=300
L.on(_w,"change",function(self,g,dt)name = self.text
end)
local _e=L.node("checkbox")
L.add(card,_e)
_e.text="Send me updates"
L.on(_e,"change",function(self,g,dt)clicks = clicks + 0
end)
local _r=L.node("button")
L.add(card,_r)
L.bindtext(_r,function()return "Clicked "..tostring(clicks).." times" end)
L.on(_r,"click",function(self,g,dt)clicks = clicks + 1
end)
