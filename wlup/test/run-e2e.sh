#!/bin/bash
# End-to-end test of the wlup prototype, run inside the xrdp-wlup-dev image:
#
#   xfreerdp3 (in Xvfb) --RDP--> xrdp + libwlup --Wayland--> headless sway
#
# 1. starts xrdp-sesman and xrdp with a [Wayland] session (code=30) that
#    loads libwlup
# 2. connects xfreerdp3 at 1280x800 as user 'tester' with a password:
#    sesman authenticates the user with PAM, starts sway headless at the
#    client's size through startwayland.sh, and passes xrdp a connection
#    to it. The user's sway config runs foot (terminal) and wev (prints
#    every input event it gets) side by side
# 3. checks sway's output has the client's size
# 4. types a command into foot and checks that it ran in the sway session
# 5. clicks and scrolls over wev and checks the events wev received
# 6. resizes the client window to 1600x900: sway's output must follow,
#    and input must still land where the client points
# Also the clipboard (clipboard-tests.sh), drive redirection and the
# microphone (drive-mic-tests.sh), and audio, logind and the microphone
# with systemd as PID 1 (wlup/test/systemd).
#
# Drive redirection needs /dev/fuse. Run the plain container with
#   --device /dev/fuse --cap-add SYS_ADMIN --security-opt apparmor:unconfined
# (the systemd one is privileged anyway); without it these checks are
# skipped.
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
./configure --enable-wlup --enable-fuse --prefix=/usr --sysconfdir=/etc --localstatedir=/var \
    CFLAGS="-O2 -g -Wno-error=nonnull" > "$OUT/configure.log" 2>&1
make -j"$(nproc)" > "$OUT/make.log" 2>&1
make install > "$OUT/install.log" 2>&1
ldconfig

# --- the test user and their sway config -------------------------------------
echo 'tester:wlup-test-pw' | chpasswd
# Focus only changes on a click, so the click test cannot pass by pointer
# motion alone
mkdir -p /home/tester/.config/sway
cat > /home/tester/.config/sway/config <<'SWAYCONF'
output * bg #204a87 solid_color
focus_follows_mouse no
exec foot
exec sh -c 'sleep 1; exec stdbuf -oL wev > /tmp/wev.log 2>&1'
SWAYCONF
chown -R tester /home/tester/.config

# sway's IPC: swaymsg talks to sway over this socket, which is found once
# sesman has started the session
SWAYSOCK=
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

# --- xrdp ---------------------------------------------------------------------
openssl req -x509 -newkey rsa:2048 -nodes -days 1 -subj /CN=wlup-test \
    -keyout /etc/xrdp/key.pem -out /etc/xrdp/cert.pem > /dev/null 2>&1
cat >> /etc/xrdp/xrdp.ini <<EOF

[Wayland]
name=Wayland (wlup prototype)
lib=libwlup.so
username=ask
password=ask
port=-1
code=30
xkb_layout=us
EOF
# Skip the login screen and go straight to the Wayland session
sed -i 's/^#\?autorun=.*/autorun=Wayland/' /etc/xrdp/xrdp.ini
sed -i 's/^LogLevel=.*/LogLevel=DEBUG/' /etc/xrdp/xrdp.ini
sed -i 's/^LogLevel=.*/LogLevel=DEBUG/' /etc/xrdp/sesman.ini
xrdp-sesman --nodaemon > "$OUT/sesman.log" 2>&1 &
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
. "$SRC/wlup/test/drive-mic-tests.sh"
drive_mic_setup
# XFREERDP: client binary, e.g. a FreeRDP built from source
${XFREERDP:-xfreerdp3} /v:127.0.0.1 /u:tester /p:wlup-test-pw /cert:ignore /size:1280x800 \
    /bpp:$BPP /dynamic-resolution -grab-keyboard /sound:sys:fake $DRIVE_MIC_OPTS \
    /log-filters:com.freerdp.channels.rdpsnd.client:DEBUG > "$OUT/client.log" 2>&1 &
CLIENT=$!
sleep 8
SWAYSOCK=$(find /run /tmp -name 'sway-ipc.*.sock' 2>/dev/null | head -1)
echo "sway IPC socket: $SWAYSOCK"
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

# --- clipboard, both ways, while foot has the focus -------------------------
. "$SRC/wlup/test/clipboard-tests.sh"
clipboard_tests

# --- drive redirection, from the same terminal ------------------------------
drive_test

# --- audio, when the user has a PipeWire daemon (systemd as PID 1) ----------
if [ "$(cat /proc/1/comm)" = systemd ] && command -v pw-cat > /dev/null; then
    as_session() {
        su tester -c "export XDG_RUNTIME_DIR=/run/user/$(id -u tester); $*"
    }
    for _ in $(seq 20); do
        as_session "pw-cli ls Node" 2>/dev/null | grep -q 'node.name = "xrdp-sink"' && break
        sleep 1
    done
    as_session "pw-cli ls Node" > "$OUT/pw-nodes.txt" 2>&1 || true
    as_session "timeout 3 pw-cat --playback --raw --target xrdp-sink \
        --format s16 --rate 44100 --channels 2 - < /dev/urandom" \
        > "$OUT/pw-cat.log" 2>&1 || true
    sleep 2
    cp "$OUT/client.log" "$OUT/client-audio.log"
    mic_test
fi

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

# --- reconnect: a new connection must reach the same session ---------------
SWAY_PID=$(pgrep -u tester -x sway)
kill $CLIENT 2>/dev/null || true
sleep 3
${XFREERDP:-xfreerdp3} /v:127.0.0.1 /u:tester /p:wlup-test-pw /cert:ignore \
    /size:1600x900 /bpp:$BPP -grab-keyboard > "$OUT/client-reconnect.log" 2>&1 &
CLIENT=$!
sleep 8
SWAY_PID_AFTER=$(pgrep -u tester -x sway)
import -window root "$OUT/client-5-reconnected.png"
read -r FOOT_X FOOT_Y < <(centre foot)
xdotool mousemove "$FOOT_X" "$FOOT_Y" click 1
sleep 0.5
xdotool type --delay 80 'echo again-$((6*7)) > /tmp/typed3.txt'
xdotool key Return
sleep 2

echo "=== result ==="
echo "sway output: connected $SIZE_CONNECTED, resized $SIZE_RESIZED"
pass=0
fail=0
check() {
    if eval "$2"; then
        echo "PASS  $1"; pass=$((pass + 1))
    else
        echo "FAIL  $1"; fail=$((fail + 1))
    fi
}
check "sesman: sway runs as the logged-in user" \
    'pgrep -u tester -x sway > /dev/null'
check "connect: output has the client size 1280x800" \
    '[ "$SIZE_CONNECTED" = 1280x800 ]'
check "keyboard: typed command ran in sway" \
    '[ "$(cat /tmp/typed.txt 2>/dev/null)" = wlup-typed-42 ]'
clipboard_checks
drive_mic_checks
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
check "reconnect: same sway process" \
    '[ -n "$SWAY_PID" ] && [ "$SWAY_PID" = "$SWAY_PID_AFTER" ]'
check "reconnect: input works in the reconnected session" \
    '[ "$(cat /tmp/typed3.txt 2>/dev/null)" = again-42 ]'

# With systemd as PID 1 (wlup/test/systemd), sesman's PAM login goes
# through pam_systemd: the session must be a real logind session
if [ "$(cat /proc/1/comm)" = systemd ]; then
    SWAY_ENV=$(tr '\0' '\n' < /proc/"$SWAY_PID_AFTER"/environ)
    SWAY_RUNTIME=$(echo "$SWAY_ENV" | sed -n 's/^XDG_RUNTIME_DIR=//p')
    SWAY_SESSION=$(echo "$SWAY_ENV" | sed -n 's/^XDG_SESSION_ID=//p')
    loginctl list-sessions --no-legend > "$OUT/loginctl-sessions.txt" 2>&1 || true
    [ -n "$SWAY_SESSION" ] &&
        loginctl show-session "$SWAY_SESSION" > "$OUT/loginctl-session.txt" 2>&1
    echo "sway: XDG_RUNTIME_DIR=$SWAY_RUNTIME XDG_SESSION_ID=$SWAY_SESSION"
    check "logind: sway's runtime dir is /run/user/<uid>" \
        '[ "$SWAY_RUNTIME" = "/run/user/$(id -u tester)" ]'
    check "logind: sway is in a logind session of tester" \
        'grep -q "^Name=tester$" "$OUT/loginctl-session.txt" &&
         grep -q "^State=active$\|^State=online$" "$OUT/loginctl-session.txt"'
    check "logind: sway's process belongs to that session" \
        '[ "$(cat /proc/$SWAY_PID_AFTER/sessionid)" = "$SWAY_SESSION" ] ||
         grep -q "session-$SWAY_SESSION.scope" /proc/$SWAY_PID_AFTER/cgroup'
    check "logind: Wayland socket in /run/user/<uid>" \
        'ls /run/user/$(id -u tester)/wayland-* > /dev/null 2>&1'
    check "audio: chansrv runs in the session" \
        'pgrep -u tester -x xrdp-chansrv > /dev/null'
    check "audio: the session's PipeWire has the xrdp sink" \
        'grep -q "node.name = \"xrdp-sink\"" "$OUT/pw-nodes.txt"'
    check "audio: the client received sound (10+ Wave PDUs)" \
        '[ "$(grep -c "WaveInfo:" "$OUT/client-audio.log")" -ge 10 ]'
fi
echo "passed $pass, failed $fail"

kill $CLIENT 2>/dev/null || true
cp /var/log/xrdp.log "$OUT/xrdp-file.log" 2>/dev/null || true
cp /var/log/xrdp-sesman.log "$OUT/sesman-file.log" 2>/dev/null || true
chmod -R a+r "$OUT"
grep -h -E 'wlup|resize_done|Advancing' "$OUT"/xrdp*.log | head -30 || true
grep -h -i -E 'wayland|compositor' "$OUT"/sesman*.log | head -20 || true
