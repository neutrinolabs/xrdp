# GNOME Shell on top of the wlup test bed, to see what a headless
# gnome-shell offers to a Wayland client such as wlup
FROM xrdp-wlup-dev

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        gnome-shell wayland-utils dbus dbus-user-session pipewire wireplumber \
    && rm -rf /var/lib/apt/lists/*
