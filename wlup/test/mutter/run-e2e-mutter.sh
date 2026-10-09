#!/bin/bash
# End-to-end test of the wlup Mutter backend:
#
#   xfreerdp3 (in Xvfb) --RDP--> xrdp + libwlup (backend=mutter)
#       --D-Bus/PipeWire/EIS--> headless gnome-shell
#
# gnome-shell needs systemd and logind, so this runs inside a container
# whose PID 1 is systemd. From the host:
#
#   docker run -d --name mutter-e2e --privileged --cgroupns=private \
#       --tmpfs /run --tmpfs /run/lock -v <xrdp src>:/src:ro -v <out>:/out \
#       xrdp-wlup-mutter /usr/lib/systemd/systemd
#   docker exec mutter-e2e bash /src/wlup/test/mutter/run-e2e-mutter.sh /src /out
#   docker rm -f mutter-e2e
#
# Spike limitation: xrdp itself runs as the session user, because Mutter's
# APIs are on that user's session bus and the frames on that user's
# PipeWire daemon.
set -eu

SRC=$1
OUT=$2
W=1280
H=800
USER_RUN=/run/user/1000
USER_ENV="XDG_RUNTIME_DIR=$USER_RUN DBUS_SESSION_BUS_ADDRESS=unix:path=$USER_RUN/bus"
mkdir -p "$OUT"

as_user() {
    su -l tester -c "export $USER_ENV; $*"
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

# --- the user's systemd instance: session bus, PipeWire, WirePlumber ---------
loginctl enable-linger tester
for _ in $(seq 50); do
    systemctl is-active -q user@1000.service && [ -S $USER_RUN/bus ] && break
    sleep 0.2
done
as_user "systemctl --user start pipewire.service wireplumber.service"

# GNOME's pointer starts at (0,0), the hot corner, and the first motion
# then opens the Activities overview, which takes the keyboard
as_user "gsettings set org.gnome.desktop.interface enable-hot-corners false"

# --- headless gnome-shell, no monitor until xrdp records a virtual one --------
as_user "nohup gnome-shell --headless --wayland-display wayland-rdp \
    > /tmp/gnome-shell.log 2>&1 &"
for _ in $(seq 100); do
    as_user "busctl --user status org.gnome.Mutter.RemoteDesktop" > /dev/null 2>&1 && break
    sleep 0.2
done
sleep 3

# --- xrdp as the session user, on port 3390 -----------------------------------
openssl req -x509 -newkey rsa:2048 -nodes -days 1 -subj /CN=wlup-test \
    -keyout /etc/xrdp/key.pem -out /etc/xrdp/cert.pem > /dev/null 2>&1
chmod 755 /etc/xrdp
chmod 644 /etc/xrdp/key.pem /etc/xrdp/cert.pem /etc/xrdp/rsakeys.ini
mkdir -p /var/run/xrdp
chown tester /var/run/xrdp
cat >> /etc/xrdp/xrdp.ini <<EOF

[GNOME]
name=GNOME (wlup prototype)
lib=libwlup.so
backend=mutter
EOF
sed -i 's/^port=3389/port=3390/; s/^#\?autorun=.*/autorun=GNOME/' /etc/xrdp/xrdp.ini
sed -i 's/^LogLevel=.*/LogLevel=DEBUG/; s|^LogFile=xrdp.log|LogFile=/tmp/xrdp.log|' /etc/xrdp/xrdp.ini
as_user "nohup /usr/sbin/xrdp --nodaemon > /tmp/xrdp-stdout.log 2>&1 &"
sleep 2

# --- RDP client in Xvfb -------------------------------------------------------
Xvfb :99 -screen 0 ${W}x${H}x24 > /dev/null 2>&1 &
export DISPLAY=:99
sleep 1
xfreerdp3 /v:127.0.0.1:3390 /u:tester /p:x /cert:ignore /size:${W}x${H} \
    /bpp:24 -grab-keyboard > "$OUT/client.log" 2>&1 &
CLIENT=$!
sleep 8
import -window root "$OUT/client-1-connected.png"

# Leave the overview gnome-shell shows at startup
xdotool key Escape
sleep 2
import -window root "$OUT/client-2-desktop.png"

# --- keyboard: type into a terminal -------------------------------------------
as_user "WAYLAND_DISPLAY=wayland-rdp nohup foot > /tmp/foot.log 2>&1 &"
sleep 3
import -window root "$OUT/client-3a-foot-started.png"
# New windows are centred; click to make sure it has focus
xdotool mousemove $((W / 2)) $((H / 2)) click 1
sleep 1
xdotool type --delay 80 'echo mutter-typed-$((6*7)) > /tmp/typed.txt'
xdotool key Return
sleep 2
import -window root "$OUT/client-3-typed.png"
as_user "pkill -x foot" || true
sleep 1

# --- pointer: buttons and wheel over wev ------------------------------------
as_user "WAYLAND_DISPLAY=wayland-rdp nohup stdbuf -oL wev > /tmp/wev.log 2>&1 &"
sleep 3
xdotool mousemove $((W / 2)) $((H / 2))
sleep 0.5
as_user ": > /tmp/wev.log"
xdotool click 1; sleep 0.3
xdotool click 3; sleep 0.3
xdotool click 2; sleep 0.3
xdotool click 4; sleep 0.3
xdotool click 5; sleep 0.3
sleep 1
import -window root "$OUT/client-4-pointer.png"
cp /tmp/wev.log /tmp/gnome-shell.log /tmp/xrdp.log /tmp/xrdp-stdout.log \
    /tmp/foot.log "$OUT/" 2>/dev/null || true

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
check "mutter: PipeWire stream at the client size" \
    'grep -q "stream format BGR[xA] ${W}x${H}" /tmp/xrdp.log'
check "mutter: EIS keyboard and pointer" \
    'grep -q "EIS keyboard" /tmp/xrdp.log && grep -q "EIS pointer" /tmp/xrdp.log'
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
echo "passed $pass, failed $fail"

kill $CLIENT 2>/dev/null || true
chmod -R a+r "$OUT"
grep -h -E 'wlup' /tmp/xrdp.log | head -20 || true
