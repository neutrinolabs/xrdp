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
cat > /tmp/sway.conf <<EOF
output HEADLESS-1 mode ${W}x${H} bg #204a87 solid_color
exec foot
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
xdotool mousemove 640 400 click 1
sleep 0.5
xdotool type --delay 80 'echo wlup-typed-$((6*7)) > /tmp/typed.txt'
xdotool key Return
sleep 2
import -window root "$OUT/client-2-typed.png"

echo "=== result ==="
if [ -f /tmp/typed.txt ]; then
    echo "typed.txt: $(cat /tmp/typed.txt)"
else
    echo "typed.txt: MISSING"
fi
kill $CLIENT 2>/dev/null || true
cp /var/log/xrdp.log "$OUT/xrdp-file.log" 2>/dev/null || true
grep -h -E 'wlup' "$OUT"/xrdp*.log | head -20 || true
