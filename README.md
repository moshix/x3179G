# x3179

An X11 emulator for the IBM 3179G graphics terminal, from the early 90s.
It was written at the Vienna University of Economics and Business
Administration by Robert Schischka and Michael Haberler, then reworked
by Pierre Verpeaux.

I got it building and running again on a current Mac (Apple Silicon,
64-bit) with XQuartz, and it builds on 64-bit Linux (amd64) too.

## Building

    make

`make` detects the OS. Before building:

- macOS: install XQuartz (https://www.xquartz.org).
- Linux (Debian/Ubuntu):

      sudo apt install build-essential pkg-config libxaw7-dev \
          libxmu-dev libxt-dev libxext-dev libx11-dev

  Fedora: `sudo dnf install gcc make pkgconf libXaw-devel
  libXmu-devel libXt-devel libXext-devel libX11-devel`

## Running

    ./x3179 -host moshix.tech:2300

`-host` takes `name:port`. Without a port it uses the telnet port
(23), or the `*port` resource if you set one with `-xrm '*port: N'`.
A port in `-host` wins over `*port`.

`X3179.ad` holds the colours (black background) and key bindings. It
is compiled into the binary, so there is nothing to set up. To change
a setting, put your own resources in a file and point `XENVIRONMENT`
at it, or use `-xrm`. Yours take precedence over the built-in ones.
`-bg` and `-fg` work too.

Resize the window and the text resizes with it. To start with smaller
or bigger text, add `-xrm '*fontSize: 10'` (the default is 13).

`x3179 -h` lists all the switches.

### Fonts

`-font NAME` (or `-fn`) picks the screen font. These read well and
scale smoothly when you resize the window:

    x3179 -host www.moshix.tech:2300 -font menlo
    x3179 -host www.moshix.tech:2300 -font 'luxi mono'
    x3179 -host www.moshix.tech:2300 -font 'courier 10 pitch'

All three come with XQuartz on macOS. On Linux try
`-font 'dejavu sans mono'` or `-font 'liberation mono'`. `xlsfonts`
shows what your X server has.

NAME can be:

- a family name: `menlo`, `'luxi mono'`
- a scalable XLFD (pixel size `0` or `*`):
  `'-bitstream-menlo-medium-r-normal--0-0-0-0-m-0-iso8859-1'`
- an XLFD pattern with `%d` where the pixel size goes
- a fixed-size font or alias such as `9x15`. The text then stays at
  that size when the window is resized.

An unknown font prints a warning and the default font is used. Use a
monospaced font: proportional fonts work but the columns will not
line up (you get a warning).

## Keys

    Return       newline (next input field)
    Option       Enter
    Backspace    delete the character before the cursor
    F1-F12       PF1-PF12
    Page Up/Dn   PA1/PA2

The rest are in `X3179.ad`.

## Tests

    make test

This starts x3179 on a hidden Xvfb display, checks that `-host`
rejects bad ports, connects with `-host moshix.tech:2300` (without
`XENVIRONMENT`), and checks that the screen comes up with a black
background, that resizing works, and that the keys above do what they
should. It needs a network
connection, python3, lsof and xdotool (`brew install xdotool` on
macOS; on Linux also Xvfb and the X utilities: `sudo apt install
xvfb x11-utils x11-apps xdotool lsof libxtst6`).

## What changed from the original

- builds on 64-bit macOS (fixed pointer truncation and the port number
  byte order)
- text scales with the window
- green block cursor
- Backspace works
- Return is newline and Option is Enter
- `-host name:port` sets the port on the command line
- `X3179.ad` is built in, so `XENVIRONMENT` is no longer needed

## Quick start

    x3179 -host moshix.tech:2300
