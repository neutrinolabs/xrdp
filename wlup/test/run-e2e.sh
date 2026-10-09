#!/bin/bash
# End-to-end test of the wlup prototype, run inside the xrdp-wlup-dev image:
#
#   xfreerdp3 (in Xvfb) --RDP--> xrdp + libwlup --Wayland--> headless sway
#
# 1. starts sway headless at 1024x768 with foot (terminal) and wev (prints
#    every input event it gets) side by side
# 2. starts xrdp with a [Wayland] session that loads libwlup
# 3. connects xfreerdp3 at 1280x800: sway's output must follow
# 4. types a command into foot and checks that it ran in the sway session
# 5. clicks and scrolls over wev and checks the events wev received
# 6. resizes the client window to 1600x900: sway's output must follow,
#    and input must still land where the client points
#
# Usage: run-e2e.sh <xrdp source dir> <output dir>
set -eu

SRC=$1
OUT=$2

mkdir -p "$OUT"

# --- build and install xrdp --------------------------------------------------
cp -a "$SRC" /build
cd /build
./bootstrap > "$OUT/bootstrap.log" 2>&1
# -Wno-error=nonnull: GCC 16 flags existing code in xrdp/xrdp_mm.c
./configure --enable-wlup --prefix=/usr --sysconfdir=/etc --localstatedir=/var \
    CFLAGS="-O2 -g -Wno-error=nonnull" > "$OUT/configure.log" 2>&1
make -j"$(nproc)" > "$OUT/make.log" 2>&1
make install > "$OUT/install.log" 2>&1
ldconfig

# --- headless sway as an ordinary user ---------------------------------------
export XDG_RUNTIME_DIR=/tmp/xdg
mkdir -p $XDG_RUNTIME_DIR
chown tester $XDG_RUNTIME_DIR
chmod 700 $XDG_RUNTIME_DIR
# Focus only changes on a click, so the click test cannot pass by pointer
# motion alone
cat > /tmp/sway.conf <<EOF
output HEADLESS-1 mode 1024x768 bg #204a87 solid_color
focus_follows_mouse no
exec foot
exec sh -c 'sleep 1; exec stdbuf -oL wev > /tmp/wev.log 2>&1'
EOF
su tester -c "XDG_RUNTIME_DIR=$XDG_RUNTIME_DIR WLR_BACKENDS=headless \
    WLR_RENDERER=pixman WLR_LIBINPUT_NO_DEVICES=1 \
    sway -c /tmp/sway.conf > $OUT/sway.log 2>&1 &"
for _ in $(seq 50); do
    [ -S $XDG_RUNTIME_DIR/wayland-1 ] && break
    sleep 0.2
done
sleep 2   # let the windows map
# xrdp runs as root here; the prototype just needs access to the socket
chmod 755 $XDG_RUNTIME_DIR

# sway's IPC: swaymsg talks to sway over this socket
SWAYSOCK=$(ls $XDG_RUNTIME_DIR/sway-ipc.*.sock)
swaymsg_() {
    su tester -c "swaymsg -s $SWAYSOCK $*"
}
output_size() {
    swaymsg_ -t get_outputs | jq -r '.[0].current_mode | "\(.width)x\(.height)"'
}
centre() {
    swaymsg_ -t get_tree |
        jq -r --arg id "$1" '.. | objects | select(.app_id? == $id) |
            "\(.rect.x + .rect.width / 2 | floor) \(.rect.y + .rect.height / 2 | floor)"'
}
SIZE_BEFORE=$(output_size)

# --- xrdp ---------------------------------------------------------------------
openssl req -x509 -newkey rsa:2048 -nodes -days 1 -subj /CN=wlup-test \
    -keyout /etc/xrdp/key.pem -out /etc/xrdp/cert.pem > /dev/null 2>&1
cat >> /etc/xrdp/xrdp.ini <<EOF

[Wayland]
name=Wayland (wlup prototype)
lib=libwlup.so
wayland_display=$XDG_RUNTIME_DIR/wayland-1
xkb_layout=us
EOF
# Skip the login screen and go straight to the Wayland session
sed -i 's/^#\?autorun=.*/autorun=Wayland/' /etc/xrdp/xrdp.ini
sed -i 's/^LogLevel=.*/LogLevel=DEBUG/' /etc/xrdp/xrdp.ini
xrdp --nodaemon > "$OUT/xrdp.log" 2>&1 &
sleep 2

# --- RDP client in Xvfb -------------------------------------------------------
Xvfb :99 -screen 0 1920x1080x24 > /dev/null 2>&1 &
export DISPLAY=:99
sleep 1
# BPP=32 makes xrdp compress bitmaps with planar. FreeRDP's client does
# not resize its planar decoder on a deactivate-reactivate resize
# (connection.c only calls DesktopResize and cache_resize), so growing
# the desktop then disconnects it ("planar->maxWidth 1280 < nSrcWidth
# 1600"). 24 bpp uses interleaved RLE, which is not affected
BPP=${BPP:-24}
# XFREERDP: client binary, e.g. a FreeRDP built from source
${XFREERDP:-xfreerdp3} /v:127.0.0.1 /u:tester /p:x /cert:ignore /size:1280x800 \
    /bpp:$BPP /dynamic-resolution -grab-keyboard > "$OUT/client.log" 2>&1 &
CLIENT=$!
sleep 6
SIZE_CONNECTED=$(output_size)
import -window root "$OUT/client-1-connected.png"

# --- keyboard: type a command into foot ---------------------------------------
read -r FOOT_X FOOT_Y < <(centre foot)
read -r WEV_X WEV_Y < <(centre wev)
echo "foot at $FOOT_X,$FOOT_Y  wev at $WEV_X,$WEV_Y"
xdotool mousemove "$FOOT_X" "$FOOT_Y" click 1
sleep 0.5
xdotool type --delay 80 'echo wlup-typed-$((6*7)) > /tmp/typed.txt'
xdotool key Return
sleep 2
import -window root "$OUT/client-2-typed.png"

# --- pointer: buttons and wheel over wev ------------------------------------
xdotool mousemove "$WEV_X" "$WEV_Y"
sleep 0.5
su tester -c ": > /tmp/wev.log"   # only keep events from here on
xdotool click 1; sleep 0.3   # left   -> BTN_LEFT   272
xdotool click 3; sleep 0.3   # right  -> BTN_RIGHT  273
xdotool click 2; sleep 0.3   # middle -> BTN_MIDDLE 274
xdotool click 4; sleep 0.3   # wheel up
xdotool click 5; sleep 0.3   # wheel down
sleep 1
cp /tmp/wev.log "$OUT/wev.log"

# --- resize: make the client window bigger ------------------------------------
WID=$(xdotool search --pid $CLIENT | tail -1)
xdotool windowsize "$WID" 1600 900
sleep 6
SIZE_RESIZED=$(output_size)
import -window root "$OUT/client-3-resized.png"

# Input after the resize: the pointer extent must follow the new size
read -r FOOT_X FOOT_Y < <(centre foot)
xdotool mousemove "$FOOT_X" "$FOOT_Y" click 1
sleep 0.5
xdotool type --delay 80 'echo resized-$((6*7)) > /tmp/typed2.txt'
xdotool key Return
sleep 2
import -window root "$OUT/client-4-typed-after-resize.png"

echo "=== result ==="
echo "sway output: start $SIZE_BEFORE, connected $SIZE_CONNECTED, resized $SIZE_RESIZED"
pass=0
fail=0
check() {
    if eval "$2"; then
        echo "PASS  $1"; pass=$((pass + 1))
    else
        echo "FAIL  $1"; fail=$((fail + 1))
    fi
}
check "connect: output follows client size 1280x800" \
    '[ "$SIZE_CONNECTED" = 1280x800 ]'
check "keyboard: typed command ran in sway" \
    '[ "$(cat /tmp/typed.txt 2>/dev/null)" = wlup-typed-42 ]'
check "left button press"   'grep -a -q "button: 272.*state: 1" /tmp/wev.log'
check "left button release" 'grep -a -q "button: 272.*state: 0" /tmp/wev.log'
check "right button"        'grep -a -q "button: 273" /tmp/wev.log'
check "middle button"       'grep -a -q "button: 274" /tmp/wev.log'
check "wheel: vertical axis events" 'grep -a -q "axis: 0" /tmp/wev.log'
check "wheel: both directions" \
    'grep -a "axis: 0" /tmp/wev.log | grep -q -- "value: -" &&
     grep -a "axis: 0" /tmp/wev.log | grep -v -q -- "value: -"'
check "resize: output follows client window 1600x900" \
    '[ "$SIZE_RESIZED" = 1600x900 ]'
check "resize: input still works after resize" \
    '[ "$(cat /tmp/typed2.txt 2>/dev/null)" = resized-42 ]'
echo "passed $pass, failed $fail"

kill $CLIENT 2>/dev/null || true
cp /var/log/xrdp.log "$OUT/xrdp-file.log" 2>/dev/null || true
chmod -R a+r "$OUT"
grep -h -E 'wlup|resize_done|Advancing' "$OUT"/xrdp*.log | head -30 || true
