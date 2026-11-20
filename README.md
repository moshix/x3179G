# x3179

An X11 emulator for the IBM 3179G graphics terminal, from the early 90s.
It was written at the Vienna University of Economics and Business
Administration by Robert Schischka and Michael Haberler, then reworked
by Pierre Verpeaux.

I got it building and running again on a current Mac (Apple Silicon,
64-bit) with XQuartz.

## Building

You need XQuartz (https://www.xquartz.org). Then:

    make -f Makefile.macos

The old `Makefile` is the imake output for AIX and won't work here.

## Running

    XENVIRONMENT=$PWD/X3179.ad ./x3179 -host moshix.tech -xrm '*port: 2300'

`X3179.ad` holds the colours and key bindings. Without it the keyboard
won't work properly.

Resize the window and the text resizes with it. To start with smaller
or bigger text, add `-xrm '*fontSize: 10'` (the default is 13).

## Keys

    Return       newline (next input field)
    Option       Enter
    Backspace    delete the character before the cursor
    F1-F12       PF1-PF12
    Page Up/Dn   PA1/PA2

The rest are in `X3179.ad`.

## Tests

    make -f Makefile.macos test

This starts x3179 on a hidden Xvfb display, connects to moshix.tech
port 2300, and checks that the screen comes up, that resizing works,
and that the keys above do what they should. It needs a network
connection, python3 and xdotool (`brew install xdotool`).

## What changed from the original

- builds on 64-bit macOS (fixed pointer truncation and the port number
  byte order)
- text scales with the window
- green block cursor
- Backspace works
- Return is newline and Option is Enter
