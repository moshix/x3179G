#!/bin/sh
# Smoke test: launch x3179 on a private headless Xvfb display, point it
# at a real TN3270 host, and check that
#   1. the process stays up (no crash at startup / negotiation),
#   2. a TCP connection to HOST:PORT is ESTABLISHED (also proves the
#      htons() port fix - before it, port 2300 went out as 64520),
#   3. the x3179 top-level window is mapped,
#   4. the window is not blank (the host's screen was painted).
#
# Usage: sh tests/smoke.sh [HOST] [PORT]   (default www.moshix.tech 2300)
# The emulator is started as "x3179 -host HOST:PORT".

HOST=${1:-www.moshix.tech}
PORT=${2:-2300}
DISP=:93
# X programs (Xvfb, xwininfo, xwd): XQuartz on macOS, /usr/bin on Linux
if [ -d /opt/X11/bin ]; then X11BIN=/opt/X11/bin; else X11BIN=/usr/bin; fi
cd "$(dirname "$0")/.." || exit 1

fail() { echo "FAIL: $*"; cleanup; exit 1; }
cleanup() {
    [ -n "$EMU" ] && kill "$EMU" 2>/dev/null
    [ -n "$XVFB" ] && kill "$XVFB" 2>/dev/null
    rm -f "$SHOT"
}
SHOT=$(mktemp "${TMPDIR:-/tmp}/x3179shot.XXXXXX")  # BSD + GNU mktemp

[ -x ./x3179 ] || fail "./x3179 not built (run make)"

"$X11BIN/Xvfb" $DISP -screen 0 2560x1600x24 -nolisten tcp >/dev/null 2>&1 &
XVFB=$!

# wait up to 5s for the X server to accept clients
i=0
until DISPLAY=$DISP "$X11BIN/xwininfo" -root >/dev/null 2>&1; do
    i=$((i + 1)); [ $i -gt 50 ] && fail "Xvfb did not start"; sleep 0.1
done

# -host with a bad port must exit with an error, not fall back to telnet
for bad in "$HOST:" "$HOST:abc" "$HOST:0" "$HOST:70000" "$HOST:23x"; do
    ERR=$(DISPLAY=$DISP ./x3179 -host "$bad" 2>&1 >/dev/null) &&
        fail "-host $bad was accepted"
    case $ERR in
        *"bad port"*) ;;
        *) fail "-host $bad: unexpected error: $ERR" ;;
    esac
done
echo "ok: -host rejects bad ports"

# connect using the "-host name:port" form.  No XENVIRONMENT: the
# colours and key bindings must come from the X3179.ad copy built into
# the binary, so every check below also tests that.
env -u XENVIRONMENT DISPLAY=$DISP \
    ./x3179 -host "$HOST:$PORT" >/dev/null 2>&1 &
EMU=$!

# wait up to 15s for the connection to come up
i=0
until lsof -nP -a -p $EMU -iTCP:"$PORT" -sTCP:ESTABLISHED >/dev/null 2>&1
do
    kill -0 $EMU 2>/dev/null || fail "x3179 exited during startup"
    i=$((i + 1)); [ $i -gt 150 ] && fail "no connection to $HOST:$PORT"
    sleep 0.1
done
echo "ok: connected to $HOST:$PORT"

# give the host a moment to send its first screen
sleep 3
kill -0 $EMU 2>/dev/null || fail "x3179 exited after connecting"
echo "ok: x3179 still running"

WIN=$(DISPLAY=$DISP "$X11BIN/xwininfo" -root -tree |
      awk '/"x3179": \("x3179"/ {print $1; exit}')
[ -n "$WIN" ] || fail "x3179 window not found"
echo "ok: window $WIN mapped"

# A blank (all background) window compresses to almost nothing; a painted
# 3270 screen does not.  Measured: blank 663x600 capture ~1.6KB gzipped,
# the Forum3270 login screen ~8.2KB.  4000 sits safely between them.
DISPLAY=$DISP "$X11BIN/xwd" -silent -id "$WIN" -out "$SHOT" ||
    fail "xwd capture failed"
SIZE=$(gzip -c "$SHOT" | wc -c | tr -d ' ')
[ "$SIZE" -gt 4000 ] || fail "window looks blank (gzip size $SIZE)"
echo "ok: screen painted (gzip size $SIZE)"

# background must be black, not Xt's default white.  Measured on the
# login screen: built-in resources ~356000 black / ~1000 white pixels,
# without them ~5000 black / ~367000 white.
BLK=$(python3 -I tests/xwd_bbox.py --count 000000 "$SHOT")
WHT=$(python3 -I tests/xwd_bbox.py --count ffffff "$SHOT")
[ "$BLK" -gt $((WHT * 10)) ] ||
    fail "background not black (black $BLK, white $WHT)"
echo "ok: black background without XENVIRONMENT"

# --- font scaling: enlarge the window, the text must grow with it ---
BBOX="python3 -I tests/xwd_bbox.py"
set -- $($BBOX "$SHOT")
W0=$1 H0=$2 BW0=$3 BH0=$4
echo "    before: window ${W0}x${H0}, text area ${BW0}x${BH0}"

# Xvfb has no window manager, so resizing the shell window directly is
# exactly what a user drag does: Xt resizes shell -> form -> screen.
SHELLWIN=$(DISPLAY=$DISP "$X11BIN/xwininfo" -root -tree |
	   awk '/"x3179": \("x3179"/ {print $1; exit}')
DISPLAY=$DISP xdotool windowsize "$SHELLWIN" $((W0 * 2)) $((H0 * 2)) ||
    fail "xdotool windowsize failed"
sleep 2
kill -0 $EMU 2>/dev/null || fail "x3179 crashed on resize"

DISPLAY=$DISP "$X11BIN/xwd" -silent -id "$SHELLWIN" -out "$SHOT" ||
    fail "xwd capture after resize failed"
set -- $($BBOX "$SHOT")
W1=$1 H1=$2 BW1=$3 BH1=$4
echo "    after:  window ${W1}x${H1}, text area ${BW1}x${BH1}"
[ "$W1" -ge $((W0 * 2 - 2)) ] || fail "window did not resize (${W1})"
# 2x window should give clearly bigger text; allow for discrete sizes
[ $((BW1 * 10)) -ge $((BW0 * 17)) ] ||
    fail "text width did not scale (${BW0} -> ${BW1})"
[ $((BH1 * 10)) -ge $((BH0 * 17)) ] ||
    fail "text height did not scale (${BH0} -> ${BH1})"
echo "ok: text scaled up with window"

# shrink back: text must return to (about) its original size
DISPLAY=$DISP xdotool windowsize "$SHELLWIN" "$W0" "$H0"
sleep 2
kill -0 $EMU 2>/dev/null || fail "x3179 crashed on shrink"
DISPLAY=$DISP "$X11BIN/xwd" -silent -id "$SHELLWIN" -out "$SHOT"
set -- $($BBOX "$SHOT")
BW2=$3 BH2=$4
echo "    back:   text area ${BW2}x${BH2}"
[ $((BW2 * 10)) -le $((BW0 * 11)) ] ||
    fail "text did not shrink back (${BW1} -> ${BW2}, orig ${BW0})"
echo "ok: text scaled back down"

# --- keyboard: BackSpace, Return (newline), Option (Enter) ---
# The login screen puts the cursor at the start of the empty User field.
# Screens are compared pixel-for-pixel with cmp: identical files mean
# identical text *and* identical cursor position.
snap() {
    DISPLAY=$DISP "$X11BIN/xwd" -silent -id "$SHELLWIN" -out "$1"
}
# letters via xdotool type; special keys via XTEST helper (see xkey.py)
press() {
    DISPLAY=$DISP xdotool "$@"; sleep 0.7
}
presskey() {
    DISPLAY=$DISP python3 -I tests/xkey.py "$@" || fail "xkey.py $*"
    sleep 0.7
}
K0=$SHOT.k0 KA=$SHOT.ka KB=$SHOT.kb KC=$SHOT.kc KE=$SHOT.ke KF=$SHOT.kf
# Keys go to the window under the pointer inside the focus window, so
# park the pointer over the screen (no WM on Xvfb to do it for us).
DISPLAY=$DISP xdotool mousemove --window "$SHELLWIN" 20 20
DISPLAY=$DISP xdotool windowfocus --sync "$SHELLWIN"
sleep 0.5
snap "$K0"

press type ab;              snap "$KA"
cmp -s "$K0" "$KA" && fail "typing 'ab' did not change the screen"
press type c
presskey BackSpace;         snap "$KB"
cmp -s "$KA" "$KB" || fail "'abc'+BackSpace differs from 'ab'"
echo "ok: BackSpace erased previous character and moved back"

# two more erase the field; a third at field start must do nothing
presskey BackSpace BackSpace BackSpace; snap "$KC"
cmp -s "$K0" "$KC" || fail "BackSpace at field start changed the screen"
echo "ok: BackSpace stops at start of field"

presskey Return;            snap "$KE"
cmp -s "$K0" "$KE" && fail "Return did not move the cursor"
echo "ok: Return moved cursor (newline)"

# Option = Enter.  The BBS answers an empty login with a red
# "Username is required" line, so red pixels must increase.
RED=ff4500                               # OrangeRed, x3179's IBM red
R0=$($BBOX --count $RED "$KE")
presskey Mode_switch; sleep 2;   snap "$KF"
R1=$($BBOX --count $RED "$KF")
[ "$R1" -gt "$R0" ] || fail "Option did not send Enter (red $R0 -> $R1)"
echo "ok: Option sent Enter, host replied (red pixels $R0 -> $R1)"
rm -f "$K0" "$KA" "$KB" "$KC" "$KE" "$KF"

cleanup
echo "PASS"
