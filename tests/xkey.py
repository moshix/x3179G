#!/usr/bin/env python3
"""
Press and release keys on $DISPLAY through the XTEST extension.

Used by tests/smoke.sh because Homebrew's xdotool segfaults on "key"
under Xvfb here.  Keysyms the server has no keycode for (Xvfb has no
Mode_switch, which is what XQuartz sends for Option) are bound to a
spare keycode first.

Usage: xkey.py KEYSYM [KEYSYM ...]     e.g. xkey.py BackSpace Return
"""
import ctypes
import sys
import time

X11 = ctypes.CDLL("/opt/X11/lib/libX11.dylib")
XTST = ctypes.CDLL("/opt/X11/lib/libXtst.dylib")

X11.XOpenDisplay.restype = ctypes.c_void_p
X11.XOpenDisplay.argtypes = [ctypes.c_char_p]
X11.XStringToKeysym.restype = ctypes.c_ulong
X11.XStringToKeysym.argtypes = [ctypes.c_char_p]
X11.XKeysymToKeycode.restype = ctypes.c_ubyte
X11.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
X11.XDisplayKeycodes.argtypes = [ctypes.c_void_p,
                                 ctypes.POINTER(ctypes.c_int),
                                 ctypes.POINTER(ctypes.c_int)]
X11.XKeycodeToKeysym.restype = ctypes.c_ulong
X11.XKeycodeToKeysym.argtypes = [ctypes.c_void_p, ctypes.c_ubyte,
                                 ctypes.c_int]
X11.XChangeKeyboardMapping.argtypes = [ctypes.c_void_p, ctypes.c_int,
                                       ctypes.c_int,
                                       ctypes.POINTER(ctypes.c_ulong),
                                       ctypes.c_int]
X11.XSync.argtypes = [ctypes.c_void_p, ctypes.c_int]
X11.XCloseDisplay.argtypes = [ctypes.c_void_p]
XTST.XTestFakeKeyEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint,
                                   ctypes.c_int, ctypes.c_ulong]


def keycode_for(dpy, name):
    sym = X11.XStringToKeysym(name.encode())
    if sym == 0:
        sys.exit("unknown keysym %s" % name)
    kc = X11.XKeysymToKeycode(dpy, sym)
    if kc:
        return kc
    # no keycode yet: bind the keysym to the highest unused keycode
    lo, hi = ctypes.c_int(), ctypes.c_int()
    X11.XDisplayKeycodes(dpy, ctypes.byref(lo), ctypes.byref(hi))
    for kc in range(hi.value, lo.value - 1, -1):
        if X11.XKeycodeToKeysym(dpy, kc, 0) == 0:
            X11.XChangeKeyboardMapping(dpy, kc, 1,
                                       ctypes.byref(ctypes.c_ulong(sym)), 1)
            X11.XSync(dpy, 0)
            time.sleep(0.3)     # let clients see the MappingNotify
            return kc
    sys.exit("no free keycode for %s" % name)


def main(names):
    dpy = X11.XOpenDisplay(None)
    if not dpy:
        sys.exit("cannot open display")
    for name in names:
        kc = keycode_for(dpy, name)
        XTST.XTestFakeKeyEvent(dpy, kc, 1, 0)   # press
        XTST.XTestFakeKeyEvent(dpy, kc, 0, 0)   # release
        X11.XSync(dpy, 0)
        time.sleep(0.15)
    X11.XCloseDisplay(dpy)


if __name__ == "__main__":
    main(sys.argv[1:])
