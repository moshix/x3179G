#!/bin/sh
# Local regression tests against tests/fakehost.py (no network needed).
# x3179 runs on a private headless Xvfb display.
#
#   1. colours: a field colour sent with SFE must survive an earlier
#      SA highlight/colour reset in the same write (3270BBS chat history
#      rows were all one colour because of this).
#   2. typing: a host Read Modified (the 3270BBS "is typing" poll) must
#      not lock the keyboard; characters typed after it must still land
#      in the field (they were dropped).
#   3. code page: x3179 must report CP037, the code page its tables use
#      (it reported CP297, so "!" showed as "|" and "\" as "$").
#   4. copy and paste: drag-select screen text, copy it (CLIPBOARD and
#      PRIMARY) and paste it into input fields; the host must read back
#      exactly the selected text, including [ ] \ ! and Latin-1 letters.
#
# Usage: sh tests/host_tests.sh
DISP=:94
PORT=${PORT:-23279}
if [ -d /opt/X11/bin ]; then X11BIN=/opt/X11/bin; else X11BIN=/usr/bin; fi
cd "$(dirname "$0")/.." || exit 1

# must match SPECIAL, FIELD1 and FIELD2 in tests/fakehost.py
SPECIAL='Copy![\]^{}|~#@$ Aeéü'
FIELD1=401
FIELD2=721

WORK=$(mktemp -d "${TMPDIR:-/tmp}/x3179host.XXXXXX")
cleanup() {
    [ -n "$EMU" ] && kill "$EMU" 2>/dev/null
    [ -n "$HOSTPID" ] && kill "$HOSTPID" 2>/dev/null
    [ -n "$XVFB" ] && kill "$XVFB" 2>/dev/null
    rm -rf "$WORK"
}
fail() { echo "FAIL: $*"; cleanup; exit 1; }
# wait_file FILE SECONDS
wait_file() {
    i=0
    until [ -e "$1" ]; do
        i=$((i + 1)); [ $i -gt $(($2 * 10)) ] && fail "timeout: $(basename "$1")"
        sleep 0.1
    done
}
# host_rm: the host sends a Read Modified; sets RESP to the reply file
N=0
host_rm() {
    N=$((N + 1))
    echo rm > "$WORK/cmd.$N"
    RESP=$WORK/resp.$N
    wait_file "$RESP" 10
}
# field ADDRESS: text the last Read Modified returned for that field
field() { sed -n "s/^$1://p" "$RESP"; }
snap() { DISPLAY=$DISP "$X11BIN/xwd" -silent -id "$WIN" -out "$1" ||
         fail "xwd capture failed"; }
xdo() { DISPLAY=$DISP xdotool "$@"; }
presskey() {
    DISPLAY=$DISP python3 -I tests/xkey.py "$@" || fail "xkey.py $*"
    sleep 0.5
}
# drag_over RRGGBB: drag with button 1 across the text drawn in that
# colour, from inside its first character to inside its last one
drag_over() {
    snap "$WORK/drag.xwd"
    set -- $(python3 -I tests/xwd_bbox.py --where "$1" "$WORK/drag.xwd") ||
        fail "no text in colour $1"
    y=$((($2 + $4) / 2))
    xdo mousemove --window "$WIN" $(($1 + 2)) $y; sleep 0.2
    xdo mousedown 1; sleep 0.2
    xdo mousemove --window "$WIN" $((($1 + $3) / 2)) $y; sleep 0.2
    xdo mousemove --window "$WIN" $(($3 - 2)) $y; sleep 0.2
    xdo mouseup 1; sleep 0.5
}

[ -x ./x3179 ] || fail "./x3179 not built (run make)"

"$X11BIN/Xvfb" $DISP -screen 0 2560x1600x24 -nolisten tcp >/dev/null 2>&1 &
XVFB=$!
i=0
until DISPLAY=$DISP "$X11BIN/xwininfo" -root >/dev/null 2>&1; do
    i=$((i + 1)); [ $i -gt 50 ] && fail "Xvfb did not start"; sleep 0.1
done

python3 -I tests/fakehost.py "$PORT" "$WORK" &
HOSTPID=$!
sleep 0.5

env -u XENVIRONMENT DISPLAY=$DISP ./x3179 -host "127.0.0.1:$PORT" \
    >/dev/null 2>&1 &
EMU=$!
wait_file "$WORK/READY" 15
sleep 1.5
kill -0 $EMU 2>/dev/null || fail "x3179 exited"

WIN=$(DISPLAY=$DISP "$X11BIN/xwininfo" -root -tree |
      awk '/"x3179": \("x3179"/ {print $1; exit}')
[ -n "$WIN" ] || fail "x3179 window not found"

# --- 1. colours -----------------------------------------------------
# Default palette: pink = magenta (ff00ff), turquoise = cyan (00ffff).
# Before the fix both rows came out SkyBlue and these counts were 0.
SHOT=$WORK/shot.xwd
snap "$SHOT"
PINKPX=$(python3 -I tests/xwd_bbox.py --count ff00ff "$SHOT")
CYANPX=$(python3 -I tests/xwd_bbox.py --count 00ffff "$SHOT")
BLUEPX=$(python3 -I tests/xwd_bbox.py --count 87ceeb "$SHOT")   # SkyBlue
[ "$PINKPX" -gt 100 ] ||
    fail "pink SFE field lost its colour ($PINKPX px, SkyBlue $BLUEPX px)"
[ "$CYANPX" -gt 100 ] ||
    fail "turquoise SFE field lost its colour ($CYANPX px)"
echo "ok: SFE colours survive an SA reset (pink $PINKPX, cyan $CYANPX px)"

# --- 2. typing across a host Read Modified --------------------------
xdo mousemove --window "$WIN" 20 20
xdo windowfocus --sync "$WIN"
sleep 0.5
xdo type --delay 100 ab; sleep 0.5
host_rm                            # poll #1, while "typing"
sleep 0.5
xdo type --delay 100 cd; sleep 0.5
host_rm                            # poll #2: must hold all four
GOT=$(field $FIELD1)
[ "$GOT" = "abcd" ] ||
    fail "typed 'abcd' around a Read Modified, host got '$GOT'"
echo "ok: keyboard stays unlocked across a host Read Modified ('$GOT')"

# --- 3. code page ---------------------------------------------------
CPID=$(cat "$WORK/CPID")
[ "$CPID" = 37 ] || fail "x3179 reports code page $CPID, expected 37"
echo "ok: x3179 reports code page 037"

# --- 4. copy and paste ----------------------------------------------
# Select the yellow SPECIAL text: it must be highlighted (reverse video
# paints the cells yellow, so many more yellow pixels), then copy it to
# CLIPBOARD with Ctrl+Shift+C.
Y0=$(python3 -I tests/xwd_bbox.py --count ffff00 "$SHOT")
drag_over ffff00
snap "$WORK/sel.xwd"
Y1=$(python3 -I tests/xwd_bbox.py --count ffff00 "$WORK/sel.xwd")
[ "$Y1" -gt $((Y0 * 3)) ] ||
    fail "dragged selection not highlighted (yellow px $Y0 -> $Y1)"
echo "ok: drag selection is highlighted (yellow px $Y0 -> $Y1)"
presskey Control_L+Shift_L+c

# A new selection, of the 40 turquoise T's, replaces PRIMARY only.
drag_over 00ffff

# Tab to field 2 and paste PRIMARY with the middle button: 39 T's fit,
# the 40th meets the protected end of the field and is dropped (no lock).
presskey Tab
xdo click 2; sleep 0.5
# Tab wraps to field 1 ("abcd"); Ctrl+Shift+V pastes CLIPBOARD over it.
presskey Tab
presskey Control_L+Shift_L+v
host_rm
GOT1=$(field $FIELD1)
GOT2=$(field $FIELD2)
TS=TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT          # 39 T's
[ "$GOT2" = "$TS" ] ||
    fail "middle-button paste of PRIMARY: field 2 got '$GOT2'"
echo "ok: drag + middle button copies and pastes (PRIMARY)"
[ "$GOT1" = "$SPECIAL" ] ||
    fail "Ctrl+Shift+C/V: field 1 got '$GOT1', expected '$SPECIAL'"
echo "ok: Ctrl+Shift+C / Ctrl+Shift+V copy and paste '$GOT1' (CLIPBOARD)"

kill -0 $EMU 2>/dev/null || fail "x3179 crashed"
echo quit > "$WORK/cmd.$((N + 1))"
cleanup
echo "PASS"
