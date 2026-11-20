#!/bin/sh
#
# Starts the Wayland compositor of an xrdp Wayland session
# (sesman.ini [Wayland] section, wlup module).
#
# sesman sets XRDP_START_WIDTH and XRDP_START_HEIGHT to the size the RDP
# client asked for. sesman finds the compositor's socket through the PID
# it started, so the compositor must be exec'd from this script.
#
# This default runs sway with the wlroots headless backend, and includes
# the user's own sway configuration (or the system one).

export WLR_BACKENDS=headless
export WLR_LIBINPUT_NO_DEVICES=1
# Software rendering, as a headless server may have no GPU
export WLR_RENDERER="${WLR_RENDERER:-pixman}"
export XDG_SESSION_TYPE=wayland

user_conf="${XDG_CONFIG_HOME:-$HOME/.config}/sway/config"
if [ ! -r "$user_conf" ]; then
    user_conf=/etc/sway/config
fi

# Audio: sway does not run XDG autostart entries, so load the PipeWire
# module for xrdp here, as GNOME would from its autostart entry. It finds
# chansrv through XRDP_PULSE_SINK_SOCKET, set by sesman
pw_xrdp=/usr/libexec/pipewire-module-xrdp/load_pw_modules.sh
if [ -x "$pw_xrdp" ]; then
    "$pw_xrdp" > /dev/null 2>&1 &
fi

conf="$XDG_RUNTIME_DIR/xrdp-sway.$$.conf"
cat > "$conf" <<END
output HEADLESS-1 mode ${XRDP_START_WIDTH:-1024}x${XRDP_START_HEIGHT:-768}
include $user_conf
END

exec sway -c "$conf"
