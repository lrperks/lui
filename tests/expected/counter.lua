local L=require"lui"
L.cfg={}
L.cfg.title="Counter"
L.cfg.width=360
L.cfg.height=160
local clicks = 0
local _w=L.node("button")
L.root(_w)
L.bindtext(_w,function()return "Clicked "..tostring(clicks).." times" end)
L.on(_w,"click",function(self,g,dt)clicks = clicks + 1
end)
