#!/bin/bash
# End-to-end test of the wlup prototype, run inside the xrdp-wlup-dev image:
#
#   xfreerdp3 (in Xvfb) --RDP--> xrdp + libwlup --Wayland--> headless sway
#
# 1. starts sway headless with a foot terminal
# 2. starts xrdp with a [Wayland] session that loads libwlup
# 3. connects xfreerdp3 and takes a screenshot of what the client shows
# 4. types a command into the remote terminal with xdotool and checks
#    that it ran inside the sway session
# 5. clicks and scrolls over a wev window and checks which pointer events
#    wev received inside the sway session
#
# Usage: run-e2e.sh <xrdp source dir> <output dir>
set -eu

SRC=$1
OUT=$2
W=1280
H=800

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
# Two tiled windows: foot on the left, wev (prints every input event it
# gets) on the right. Focus only changes on a click, so the click test
# cannot pass by pointer motion alone
cat > /tmp/sway.conf <<EOF
output HEADLESS-1 mode ${W}x${H} bg #204a87 solid_color
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
sleep 2   # let foot map its window
WAYLAND_SOCKET_PATH=$XDG_RUNTIME_DIR/wayland-1
# xrdp runs as root here; the prototype just needs access to the socket
chmod 755 $XDG_RUNTIME_DIR
su tester -c "XDG_RUNTIME_DIR=$XDG_RUNTIME_DIR WAYLAND_DISPLAY=wayland-1 \
    grim $OUT/sway-direct.png" || true

# Window centres, from sway's IPC (swaymsg talks to sway over $SWAYSOCK)
SWAYSOCK=$(ls $XDG_RUNTIME_DIR/sway-ipc.*.sock)
centre() {
    su tester -c "swaymsg -s $SWAYSOCK -t get_tree" |
        jq -r --arg id "$1" '.. | objects | select(.app_id? == $id) |
            "\(.rect.x + .rect.width / 2 | floor) \(.rect.y + .rect.height / 2 | floor)"'
}
read -r FOOT_X FOOT_Y < <(centre foot)
read -r WEV_X WEV_Y < <(centre wev)
echo "foot at $FOOT_X,$FOOT_Y  wev at $WEV_X,$WEV_Y"

# --- xrdp ---------------------------------------------------------------------
openssl req -x509 -newkey rsa:2048 -nodes -days 1 -subj /CN=wlup-test \
    -keyout /etc/xrdp/key.pem -out /etc/xrdp/cert.pem > /dev/null 2>&1
cat >> /etc/xrdp/xrdp.ini <<EOF

[Wayland]
name=Wayland (wlup prototype)
lib=libwlup.so
wayland_display=$WAYLAND_SOCKET_PATH
xkb_layout=us
EOF
# Skip the login screen and go straight to the Wayland session
sed -i 's/^#\?autorun=.*/autorun=Wayland/' /etc/xrdp/xrdp.ini
sed -i 's/^LogLevel=.*/LogLevel=DEBUG/; s/^EnableConsole=.*/EnableConsole=true/' /etc/xrdp/xrdp.ini
xrdp --nodaemon > "$OUT/xrdp.log" 2>&1 &
sleep 2

# --- RDP client in Xvfb -------------------------------------------------------
Xvfb :99 -screen 0 ${W}x${H}x24 > /dev/null 2>&1 &
export DISPLAY=:99
sleep 1
xfreerdp3 /v:127.0.0.1 /u:tester /p:x /cert:ignore /size:${W}x${H} \
    /bpp:32 -grab-keyboard > "$OUT/client.log" 2>&1 &
CLIENT=$!
sleep 6
import -window root "$OUT/client-1-connected.png"

# --- input: type a command into the remote foot terminal ---------------------
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
import -window root "$OUT/client-3-pointer.png"
cp /tmp/wev.log "$OUT/wev.log"

echo "=== result ==="
pass=0
fail=0
check() {
    if eval "$2"; then
        echo "PASS  $1"; pass=$((pass + 1))
    else
        echo "FAIL  $1"; fail=$((fail + 1))
    fi
}
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
echo "passed $pass, failed $fail"
kill $CLIENT 2>/dev/null || true
cp /var/log/xrdp.log "$OUT/xrdp-file.log" 2>/dev/null || true
grep -h -E 'wlup' "$OUT"/xrdp*.log | head -20 || true
