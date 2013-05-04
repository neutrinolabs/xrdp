#!/bin/sh
if [ -r /etc/default/locale ]; then
  . /etc/default/locale
  export LANG LANGUAGE
fi
. /etc/X11/Xsession

# debian
if [ -r /etc/X11/Xsession ]; then
  . /etc/X11/Xsession
  exit 0
fi

# el
if [ -r /etc/X11/xinit/Xsession ]; then
  . /etc/X11/xinit/Xsession
  exit 0
fi

# suse
if [ -r /etc/X11/xdm/Xsession ]; then
  . /etc/X11/xdm/Xsession
  exit 0
fi

xterm
