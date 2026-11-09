local L=require"lui"
L.cfg={}
L.cfg.title="Canvas and scroll"
L.cfg.width=520
L.cfg.height=420
L.cfg.theme="dark"
local t = 0
local view=L.node("canvas")
L.root(view)
view.width=480
view.height=200
L.on(view,"frame",function(self,g,dt)t = t + dt
end)
L.on(view,"draw",function(self,g,dt)for i = 0, 9 do
g.circle(40 + i * 45, 100 + math.sin(t * 2 + i) * 40, 14, "#22c55e")
end
g.text("Canvas time: " .. string.format("%.1f", t), 12, 12, 16, "#e5e7eb")
end)
local list=L.node("scroll")
L.root(list)
list.width=480
list.height=180
list.bg="#1c1f26"
local _w=L.node("label")
L.add(list,_w)
_w.text="Row 1"
local _e=L.node("label")
L.add(list,_e)
_e.text="Row 2"
local _r=L.node("label")
L.add(list,_r)
_r.text="Row 3"
local _t=L.node("label")
L.add(list,_t)
_t.text="Row 4"
local _y=L.node("label")
L.add(list,_y)
_y.text="Row 5"
local _u=L.node("label")
L.add(list,_u)
_u.text="Row 6"
local _i=L.node("label")
L.add(list,_i)
_i.text="Row 7"
local _o=L.node("label")
L.add(list,_o)
_o.text="Row 8"
local _p=L.node("label")
L.add(list,_p)
_p.text="Row 9"
local _a=L.node("label")
L.add(list,_a)
_a.text="Row 10"
local _s=L.node("label")
L.add(list,_s)
_s.text="Row 11"
local _d=L.node("label")
L.add(list,_d)
_d.text="Row 12"
local _f=L.node("label")
L.add(list,_f)
_f.text="Row 13"
local _g=L.node("label")
L.add(list,_g)
_g.text="Row 14"
local _h=L.node("label")
L.add(list,_h)
_h.text="Row 15"
local _j=L.node("label")
L.add(list,_j)
_j.text="Row 16"
local _k=L.node("label")
L.add(list,_k)
_k.text="Row 17"
local _l=L.node("label")
L.add(list,_l)
_l.text="Row 18"
local _z=L.node("label")
L.add(list,_z)
_z.text="Row 19"
local _x=L.node("label")
L.add(list,_x)
_x.text="Row 20"
