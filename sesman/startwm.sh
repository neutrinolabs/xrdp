#!/bin/sh

# change the order in line below to run to run whatever window manager you
# want, default to kde

SESSIONS="startkde gnome-session startxfce4 xterm"

# change PATH to be what your environment needs usually what is in
# /etc/environment
#PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games"
#export PATH=$PATH

for WindowManager in $SESSIONS
do
  which $WindowManager
  if test $? -eq 0
  then
    echo "Starting $WindowManager"
    $WindowManager
    exit 0
  fi
done

exit 1
