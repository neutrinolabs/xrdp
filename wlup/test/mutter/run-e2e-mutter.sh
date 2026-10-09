#!/bin/bash
# End-to-end test of a GNOME session through sesman and the wlup Mutter
# backend:
#
#   xfreerdp3 (in Xvfb) --RDP--> xrdp + libwlup (backend=mutter)
#       --session bus/PipeWire/EIS--> GNOME session started by sesman
#
# xrdp and sesman run as root. The client logs in as 'tester' with a
# password; sesman starts gnome-session for the user (sesman.ini [GNOME])
# and passes xrdp connections to the user's session bus and PipeWire
# daemon, made as the user.
#
# GNOME needs systemd and logind, so this runs inside a container whose
# PID 1 is systemd. From the host:
#
#   docker run -d --name gnome-e2e --privileged --cgroupns=private \
#       --tmpfs /run --tmpfs /run/lock -v <xrdp src>:/src:ro -v <out>:/out \
#       xrdp-wlup-mutter /usr/lib/systemd/systemd
#   docker exec gnome-e2e bash /src/wlup/test/mutter/run-e2e-mutter.sh /src /out
#   docker rm -f gnome-e2e
#
# Usage: run-e2e-mutter.sh <xrdp source dir> <output dir>
set -eu

SRC=$1
OUT=$2
W=1280
H=800
PASSWORD=wlup-test-pw
USER_RUN=/run/user/1000
mkdir -p "$OUT"

# Runs a command as the user inside their GNOME session
in_session() {
    local wl
    wl=$(cd $USER_RUN && ls wayland-? 2>/dev/null | head -1)
    su tester -c "export XDG_RUNTIME_DIR=$USER_RUN \
        DBUS_SESSION_BUS_ADDRESS=unix:path=$USER_RUN/bus WAYLAND_DISPLAY=$wl; $*"
}

# --- build and install xrdp --------------------------------------------------
cp -a "$SRC" /build
cd /build
./bootstrap > "$OUT/bootstrap.log" 2>&1
./configure --enable-wlup --enable-wlup-mutter --prefix=/usr --sysconfdir=/etc \
    --localstatedir=/var CFLAGS="-O2 -g -Wno-error=nonnull" > "$OUT/configure.log" 2>&1
make -j"$(nproc)" > "$OUT/make.log" 2>&1
make install > "$OUT/install.log" 2>&1
ldconfig

# --- the test user -------------------------------------------------------------
echo "tester:$PASSWORD" | chpasswd
# GNOME's pointer starts at (0,0), the hot corner, and the first motion
# then opens the Activities overview, which takes the keyboard
su -l tester -c "dbus-run-session -- gsettings set \
    org.gnome.desktop.interface enable-hot-corners false"

# --- xrdp and sesman, as root --------------------------------------------------
openssl req -x509 -newkey rsa:2048 -nodes -days 1 -subj /CN=wlup-test \
    -keyout /etc/xrdp/key.pem -out /etc/xrdp/cert.pem > /dev/null 2>&1
cat >> /etc/xrdp/xrdp.ini <<EOF

[GNOME]
name=GNOME (wlup prototype)
lib=libwlup.so
username=ask
password=ask
port=-1
code=31
backend=mutter
EOF
sed -i 's/^#\?autorun=.*/autorun=GNOME/' /etc/xrdp/xrdp.ini
sed -i 's/^LogLevel=.*/LogLevel=DEBUG/' /etc/xrdp/xrdp.ini /etc/xrdp/sesman.ini
xrdp-sesman --nodaemon > "$OUT/sesman.log" 2>&1 &
xrdp --nodaemon > "$OUT/xrdp-stdout.log" 2>&1 &
sleep 2

# --- RDP client in Xvfb -------------------------------------------------------
Xvfb :99 -screen 0 1920x1080x24 > /dev/null 2>&1 &
export DISPLAY=:99
sleep 1
connect() {
    xfreerdp3 /v:127.0.0.1 /u:tester /p:$PASSWORD /cert:ignore /size:${W}x${H} \
        /bpp:24 -grab-keyboard > "$OUT/client$1.log" 2>&1 &
    CLIENT=$!
}
connect ""
# sesman starts GNOME, then wlup waits for GNOME Shell
for _ in $(seq 60); do
    grep -q "wlup: connected to Mutter" /var/log/xrdp.log 2>/dev/null && break
    sleep 1
done
sleep 4
import -window root "$OUT/client-1-connected.png"

# Leave the overview gnome-shell shows at startup
xdotool key Escape
sleep 2
import -window root "$OUT/client-2-desktop.png"

# --- who runs what -----------------------------------------------------------
SHELL_PID=$(pgrep -u tester -x gnome-shell || true)
# gnome-shell is started by the user's systemd instance, outside the
# logind session; the session is the one sesman opened for tester
SESSION_ID=$(loginctl list-sessions --no-legend |
    awk '$3 == "tester" && $0 !~ /manager/ { print $1; exit }')
loginctl show-session "${SESSION_ID:-none}" > "$OUT/loginctl-session.txt" 2>&1 || true
in_session "journalctl --user --no-pager -o cat -u 'org.gnome.Shell@*'" \
    > "$OUT/gnome-shell-journal.log" 2>&1 || true
ps -eo user,pid,ppid,cmd > "$OUT/ps.txt"

# --- keyboard: type into a terminal -------------------------------------------
in_session "nohup foot > /tmp/foot.log 2>&1 &"
sleep 3
xdotool mousemove $((W / 2)) $((H / 2)) click 1
sleep 1
xdotool type --delay 80 'echo mutter-typed-$((6*7)) > /tmp/typed.txt'
xdotool key Return
sleep 2
import -window root "$OUT/client-3-typed.png"
in_session "pkill -x foot" || true
sleep 1

# --- pointer: buttons and wheel over wev ------------------------------------
in_session "nohup stdbuf -oL wev > /tmp/wev.log 2>&1 &"
sleep 3
xdotool mousemove $((W / 2)) $((H / 2))
sleep 0.5
su tester -c ": > /tmp/wev.log"
xdotool click 1; sleep 0.3
xdotool click 3; sleep 0.3
xdotool click 2; sleep 0.3
xdotool click 4; sleep 0.3
xdotool click 5; sleep 0.3
sleep 1
import -window root "$OUT/client-4-pointer.png"
cp /tmp/wev.log "$OUT/wev.log"
in_session "pkill -x wev" || true

# --- reconnect: a new connection must reach the same GNOME session ----------
kill $CLIENT 2>/dev/null || true
sleep 3
connect "-reconnect"
for _ in $(seq 30); do
    [ "$(grep -c "wlup: connected to Mutter" /var/log/xrdp.log)" -ge 2 ] && break
    sleep 1
done
sleep 4
SHELL_PID_AFTER=$(pgrep -u tester -x gnome-shell || true)
import -window root "$OUT/client-5-reconnected.png"
in_session "nohup foot > /tmp/foot.log 2>&1 &"
sleep 3
xdotool mousemove $((W / 2)) $((H / 2)) click 1
sleep 1
xdotool type --delay 80 'echo again-$((6*7)) > /tmp/typed2.txt'
xdotool key Return
sleep 2
import -window root "$OUT/client-6-typed-after-reconnect.png"

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
check "sesman: gnome-session runs as the logged-in user" \
    'pgrep -u tester -f "gnome-session" > /dev/null'
check "sesman: gnome-shell runs as the logged-in user" \
    '[ -n "$SHELL_PID" ]'
check "xrdp runs as root, not as the user" \
    '[ "$(ps -o user= -C xrdp | sort -u)" = root ]'
check "mutter: GNOME Shell chose headless mode itself" \
    'grep -q "running headlessly" "$OUT/gnome-shell-journal.log"'
check "logind: remote wayland session of tester" \
    'grep -q "^Name=tester$" "$OUT/loginctl-session.txt" &&
     grep -q "^Type=wayland$" "$OUT/loginctl-session.txt" &&
     grep -q "^Remote=yes$" "$OUT/loginctl-session.txt"'
check "mutter: PipeWire stream at the client size" \
    'grep -q "stream format BGR[xA] ${W}x${H}" /var/log/xrdp.log'
check "mutter: EIS keyboard and pointer" \
    'grep -q "EIS keyboard" /var/log/xrdp.log &&
     grep -q "EIS pointer" /var/log/xrdp.log'
check "picture: client shows more than a flat screen" \
    '[ "$(convert "$OUT/client-2-desktop.png" -format %k info:)" -gt 50 ]'
check "keyboard: typed command ran in the GNOME session" \
    '[ "$(cat /tmp/typed.txt 2>/dev/null)" = mutter-typed-42 ]'
check "left button press"   'grep -a -q "button: 272.*state: 1" /tmp/wev.log'
check "left button release" 'grep -a -q "button: 272.*state: 0" /tmp/wev.log'
check "right button"        'grep -a -q "button: 273" /tmp/wev.log'
check "middle button"       'grep -a -q "button: 274" /tmp/wev.log'
check "wheel: vertical axis events" 'grep -a -q "axis: 0" /tmp/wev.log'
check "wheel: both directions" \
    'grep -a "axis: 0" /tmp/wev.log | grep -q -- "value: -" &&
     grep -a "axis: 0" /tmp/wev.log | grep -v -q -- "value: -"'
check "reconnect: same gnome-shell process" \
    '[ -n "$SHELL_PID" ] && [ "$SHELL_PID" = "$SHELL_PID_AFTER" ]'
check "reconnect: input works in the reconnected session" \
    '[ "$(cat /tmp/typed2.txt 2>/dev/null)" = again-42 ]'
echo "passed $pass, failed $fail"

kill $CLIENT 2>/dev/null || true
cp /var/log/xrdp.log /var/log/xrdp-sesman.log "$OUT/" 2>/dev/null || true
chmod -R a+r "$OUT"
grep -h -E 'wlup' /var/log/xrdp.log | head -20 || true
grep -h -i -E 'gnome|connect.*as user|session bus' /var/log/xrdp-sesman.log | head -20 || true
