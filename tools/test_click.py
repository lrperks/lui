#!/usr/bin/env python3
"""Clicks the counter button in a running lui window and checks the handler ran.

Usage: python3 tools/test_click.py examples/counter.lui
Clicks through XTest (libXtst, via ctypes) and reads the host log, which
lui.lua writes to stderr when LUI_DEBUG is set.
"""
import ctypes, os, re, subprocess, sys, time

app = sys.argv[1]
x11 = ctypes.CDLL("libX11.so.6")
xtst = ctypes.CDLL("libXtst.so.6")
x11.XOpenDisplay.restype = ctypes.c_void_p
x11.XDefaultScreen.argtypes = [ctypes.c_void_p]
x11.XFlush.argtypes = [ctypes.c_void_p]
xtst.XTestFakeMotionEvent.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_ulong]
xtst.XTestFakeButtonEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
host = subprocess.Popen([os.path.join(ROOT, "bin/lui"), "run", app], env={**os.environ, "LUI_DEBUG": "1"}, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
time.sleep(2.0)

# The client window is the child named "Counter" (the WM frame has the same
# name); its absolute origin is the last +X+Y pair on that tree line.
tree = subprocess.run(["xwininfo", "-root", "-tree"], capture_output=True, text=True).stdout
line = next(l for l in tree.splitlines() if '("Counter" "Counter")' in l)
wx, wy = map(int, re.search(r"\+(-?\d+)\+(-?\d+)\s*$", line).groups())

dpy = x11.XOpenDisplay(None)
screen = x11.XDefaultScreen(dpy)
def click_at(x, y):
    xtst.XTestFakeMotionEvent(dpy, screen, x, y, 0)
    x11.XFlush(dpy); time.sleep(0.2)
    xtst.XTestFakeButtonEvent(dpy, 1, 1, 0); x11.XFlush(dpy); time.sleep(0.05)
    xtst.XTestFakeButtonEvent(dpy, 1, 0, 0); x11.XFlush(dpy); time.sleep(0.3)

BUTTON = (40, 40)  # window-relative point inside the button
for _ in range(2):
    click_at(wx + BUTTON[0], wy + BUTTON[1])

host.terminate()
err = host.stderr.read().decode()
print("host log:\n" + (err.strip() or "(none)"))
clicks = [l for l in err.splitlines() if l.startswith("click")]
print("PASS" if len(clicks) == 2 else "FAIL")
