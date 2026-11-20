#!/bin/sh
#
# Starts the GNOME session of an xrdp GNOME session
# (sesman.ini [GNOME] section, wlup module with backend=mutter).
#
# gnome-session must be exec'd from this script: sesman watches the PID it
# started, and the session ends when that process exits.
#
# This sets the variables a display manager sets for a GNOME Wayland
# session. XDG_SESSION_TYPE=wayland is also what logind recorded when
# sesman opened the session, which GNOME Shell needs to find it.

export XDG_SESSION_TYPE=wayland
export XDG_CURRENT_DESKTOP=GNOME
export XDG_SESSION_DESKTOP=gnome
export DESKTOP_SESSION=gnome

exec gnome-session
