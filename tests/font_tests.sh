#!/bin/sh
# Font switch tests (no network needed).  x3179 runs on a private
# headless Xvfb display against a dummy host that accepts the TCP
# connection and stays silent, so only the window and fonts matter.
#
#   1. -h lists the switches, exits 0, needs no display, and every
#      line fits in 79 columns.
#   2. -font takes a family name, a scalable XLFD, a %d pattern and a
#      fixed-size alias; each starts without a warning.
#   3. a fixed-size alias (9x15) really changes the font: the window
#      width becomes 80 * 9 + margins instead of the default.
#   4. an unknown font warns and falls back to the default.
#   5. a user font still lets the window take any -geometry.
#
# Fonts used are the default family (bitstream vera sans mono) and
# 9x15, so the test does not depend on extra fonts being installed.
#
# Usage: sh tests/font_tests.sh
DISP=:97
PORT=${PORT:-23280}
if [ -d /opt/X11/bin ]; then X11BIN=/opt/X11/bin; else X11BIN=/usr/bin; fi
cd "$(dirname "$0")/.." || exit 1

WORK=$(mktemp -d "${TMPDIR:-/tmp}/x3179font.XXXXXX")
cleanup() {
    [ -n "$EMU" ] && kill "$EMU" 2>/dev/null
    [ -n "$HOSTPID" ] && { kill "$HOSTPID"; wait "$HOSTPID"; } 2>/dev/null
    [ -n "$XVFB" ] && kill "$XVFB" 2>/dev/null
    rm -rf "$WORK"
}
fail() { echo "FAIL: $*"; cleanup; exit 1; }

[ -x ./x3179 ] || fail "./x3179 not built (run make)"

# --- 1. -h ----------------------------------------------------------
env -u DISPLAY ./x3179 -h > "$WORK/help" 2>&1 || fail "-h did not exit 0"
grep -q -- "-font NAME" "$WORK/help" || fail "-h does not list -font"
LONG=$(awk 'length > 79' "$WORK/help")
[ -z "$LONG" ] || fail "-h line longer than 79 columns: $LONG"
echo "ok: -h lists the switches within 79 columns"

"$X11BIN/Xvfb" $DISP -screen 0 2560x1600x24 -nolisten tcp >/dev/null 2>&1 &
XVFB=$!
i=0
until DISPLAY=$DISP "$X11BIN/xwininfo" -root >/dev/null 2>&1; do
    i=$((i + 1)); [ $i -gt 50 ] && fail "Xvfb did not start"; sleep 0.1
done

# dummy host: accept every connection and keep it open, send nothing
python3 -I -c '
import socket, sys
s = socket.socket(); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("127.0.0.1", int(sys.argv[1]))); s.listen(8)
keep = []
while True:
    keep.append(s.accept()[0])
' "$PORT" &
HOSTPID=$!
sleep 0.5

# run_font ARGS...: start x3179, set WIDTH/HEIGHT and ERR, stop it
run_font() {
    env -u XENVIRONMENT DISPLAY=$DISP ./x3179 -host "127.0.0.1:$PORT" \
        "$@" > "$WORK/err" 2>&1 &
    EMU=$!
    WIN=""
    i=0
    while [ -z "$WIN" ]; do
        kill -0 $EMU 2>/dev/null || fail "x3179 $* exited: $(cat "$WORK/err")"
        i=$((i + 1)); [ $i -gt 50 ] && fail "x3179 $*: no window"
        sleep 0.1
        WIN=$(DISPLAY=$DISP "$X11BIN/xwininfo" -root -tree |
              awk '/"x3179": \("x3179"/ {print $1; exit}')
    done
    sleep 0.5
    set -- $(DISPLAY=$DISP "$X11BIN/xwininfo" -id "$WIN" |
             awk '/Width:/ {w = $2} /Height:/ {h = $2} END {print w, h}')
    WIDTH=$1; HEIGHT=$2
    ERR=$(cat "$WORK/err")
    kill $EMU; wait $EMU 2>/dev/null; EMU=""
}

# --- 2. accepted forms ----------------------------------------------
run_font
DEF_W=$WIDTH; DEF_H=$HEIGHT
VERA='bitstream vera sans mono'
for f in "$VERA" \
    "-bitstream-$VERA-medium-r-normal--0-0-0-0-m-0-iso8859-1" \
    "-*-$VERA-medium-r-normal--%d-0-0-0-m-0-iso8859-1" 9x15; do
    run_font -font "$f"
    [ -z "$ERR" ] || fail "-font '$f' warned: $ERR"
done
echo "ok: -font accepts family, XLFD, %d pattern and alias"

# --- 3. fixed alias changes the size --------------------------------
run_font -font 9x15
[ "$WIDTH" -ne "$DEF_W" ] || fail "-font 9x15 left width at $WIDTH"
[ "$WIDTH" -ge 720 ] && [ "$WIDTH" -lt 760 ] ||
    fail "-font 9x15: width $WIDTH, expected 80 * 9 + margins"
run_font -fn 9x15
[ "$WIDTH" -ge 720 ] && [ "$WIDTH" -lt 760 ] || fail "-fn 9x15 ignored"
echo "ok: -font/-fn 9x15 gives a ${WIDTH}px wide window (default $DEF_W)"

# --- 4. unknown font ------------------------------------------------
run_font -font no-such-font-xyz
case $ERR in
    *"not found; using the default font"*) ;;
    *) fail "unknown font: unexpected stderr '$ERR'" ;;
esac
[ "$WIDTH" -eq "$DEF_W" ] || fail "unknown font changed width to $WIDTH"
echo "ok: unknown font warns and keeps the default"

# --- 5. user font + -geometry --------------------------------------
run_font -font "$VERA" -geometry 1300x1050
[ "$WIDTH" -eq 1300 ] || fail "-font + -geometry: width $WIDTH"
echo "ok: -font window takes -geometry 1300x1050"

cleanup
echo "PASS"
