local L=require"lui"
L.cfg={}
L.cfg.title="Lui widgets"
L.cfg.width=980
L.cfg.height=720
L.cfg.accent="#8b5cf6"
local volume = 0.6
local name = ""
local log = {}
local function note(s)
table.insert(log, 1, os.date("%H:%M:%S  ") .. s)
if #log > 8 then table.remove(log) end
end
local _w=L.node("menubar")
L.root(_w)
local _e=L.node("menu")
L.add(_w,_e)
_e.label="File"
local _r=L.node("item")
L.add(_e,_r)
_r.label="New"
_r.shortcut="Ctrl+N"
L.on(_r,"click",function(self,g,dt)note("New file")
end)
local _t=L.node("item")
L.add(_e,_t)
_t.label="Open…"
_t.shortcut="Ctrl+O"
L.on(_t,"click",function(self,g,dt)note("Open")
end)
local _y=L.node("separator")
L.add(_e,_y)
local _u=L.node("item")
L.add(_e,_u)
_u.label="Quit"
L.on(_u,"click",function(self,g,dt)os.exit(0)
end)
local _i=L.node("menu")
L.add(_w,_i)
_i.label="View"
local _o=L.node("item")
L.add(_i,_o)
_o.label="Dark mode"
_o.checked=true
local _p=L.node("tabs")
L.root(_p)
local _a=L.node("tab")
L.add(_p,_a)
_a.label="Controls"
local _s=L.node("separator")
L.add(_a,_s)
_s.text="Input"
local _d=L.node("input")
L.add(_a,_d)
_d.placeholder="Your name"
L.on(_d,"change",function(self,g,dt)name = self.text
end)
local _f=L.node("text")
L.add(_a,_f)
L.bindtext(_f,function()return "Hello, "..tostring(name ~= '' and name or 'stranger').."!" end)
local _g=L.node("separator")
L.add(_a,_g)
_g.text="Values"
local _h=L.node("slider")
L.add(_a,_h)
_h.label="Volume"
_h.value=0.6
_h.min=0
_h.max=1
L.on(_h,"change",function(self,g,dt)volume = self.value
end)
local _j=L.node("progress")
L.add(_a,_j)
L.bind(_j,"value",function()return volume end)
L.bindtext(_j,function()return tostring(math.floor(volume * 100)).."%" end)
local _k=L.node("drag")
L.add(_a,_k)
_k.label="Speed"
_k.value=1.5
_k.speed=0.05
_k.min=0
_k.max=10
local _l=L.node("combo")
L.add(_a,_l)
_l.label="Quality"
L.bind(_l,"items",function()return  {"Low", "Medium", "High", "Ultra"}  end)
_l.selected=3
L.on(_l,"change",function(self,g,dt)note("Quality: " .. self.items[self.selected])
end)
local _z=L.node("color")
L.add(_a,_z)
_z.label="Accent"
_z.value="#8b5cf6"
local _x=L.node("separator")
L.add(_a,_x)
_x.text="Choices"
local _c=L.node("row")
L.add(_a,_c)
local _v=L.node("checkbox")
L.add(_c,_v)
_v.label="Notifications"
_v.checked=true
L.on(_v,"change",function(self,g,dt)note("Notifications " .. (self.checked and "on" or "off"))
end)
local _b=L.node("checkbox")
L.add(_c,_b)
_b.label="Auto-update"
local _n=L.node("row")
L.add(_a,_n)
local _m=L.node("button")
L.add(_n,_m)
_m.tooltip="Saves your changes"
_m.text="Save"
L.on(_m,"click",function(self,g,dt)note("Saved")
end)
local _Q=L.node("button")
L.add(_n,_Q)
_Q.bg="#334155"
_Q.text="Cancel"
L.on(_Q,"click",function(self,g,dt)note("Cancelled")
end)
local _W=L.node("tab")
L.add(_p,_W)
_W.label="Data"
local _E=L.node("table")
L.add(_W,_E)
_E.columns="Name,Kind,Size"
_E.border=true
local _R=L.node("tr")
L.add(_E,_R)
local _T=L.node("td")
L.add(_R,_T)
_T.text="lui.lua"
local _Y=L.node("td")
L.add(_R,_Y)
_Y.text="runtime"
local _U=L.node("td")
L.add(_R,_U)
_U.text="14 KB"
local _I=L.node("tr")
L.add(_E,_I)
local _O=L.node("td")
L.add(_I,_O)
_O.text="luic.c"
local _P=L.node("td")
L.add(_I,_P)
_P.text="compiler"
local _A=L.node("td")
L.add(_I,_A)
_A.text="13 KB"
local _S=L.node("tr")
L.add(_E,_S)
local _D=L.node("td")
L.add(_S,_D)
_D.text="lui_imgui.cpp"
local _F=L.node("td")
L.add(_S,_F)
_F.text="bindings"
local _G=L.node("td")
L.add(_S,_G)
_G.text="16 KB"
local _H=L.node("plot")
L.add(_W,_H)
_H.label="##fps"
_H.height=90
L.bind(_H,"values",function()return  {60, 58, 61, 59, 45, 52, 60, 62, 61, 59, 57, 60}  end)
local _J=L.node("tree")
L.add(_W,_J)
_J.label="Project"
_J.open=true
local _K=L.node("tree")
L.add(_J,_K)
_K.label="src"
local _L=L.node("selectable")
L.add(_K,_L)
_L.label="lui.lua"
local _Z=L.node("selectable")
L.add(_K,_Z)
_Z.label="luic.c"
local _X=L.node("tree")
L.add(_J,_X)
_X.label="examples"
local _C=L.node("selectable")
L.add(_X,_C)
_C.label="widgets.lui"
local _V=L.node("tab")
L.add(_p,_V)
_V.label="Canvas"
local _B=L.node("canvas")
L.add(_V,_B)
_B.height=220
_B.bg="#181b22"
L.on(_B,"draw",function(self,g,dt)local t = os.clock()
for i = 0, 11 do
local x = 40 + i * (g.width - 80) / 11
local y = g.height / 2 + math.sin(t * 3 + i * 0.6) * 60
g.circle(x, y, 10, "#8b5cf6")
end
g.text("Drawn with the canvas API", 16, 14, 15, "#9ca3af")
end)
local _N=L.node("collapse")
L.root(_N)
_N.label="Activity"
_N.open=true
local _M=L.node("scroll")
L.add(_N,_M)
_M.height=140
local _qw=L.node("text")
L.add(_M,_qw)
L.bindtext(_qw,function()return tostring(#log == 0 and 'Nothing yet — try the controls.' or table.concat(log, '\n')) end)
local _ww=L.node("window")
L.root(_ww)
_ww.title="Inspector"
_ww.width=260
_ww.height=220
local _ew=L.node("text")
L.add(_ww,_ew)
_ew.text="Floating ImGui window"
local _rw=L.node("separator")
L.add(_ww,_rw)
local _tw=L.node("text")
L.add(_ww,_tw)
L.bindtext(_tw,function()return "Volume: "..tostring(string.format('%.2f', volume)) end)
local _yw=L.node("checkbox")
L.add(_ww,_yw)
_yw.label="Pin"
