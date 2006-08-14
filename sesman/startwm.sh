#!/bin/sh

# edit this file to run whatever window manager you want
# defaults to running kde

# for kde
if [ -d /opt/kde3/bin ]; then
  export PATH=/opt/kde3/bin:$PATH
fi
if [ -d /opt/kde/bin ]; then
  export PATH=/opt/kde/bin:$PATH
fi
which startkde
if [ $? -eq 0 ]; then
  startkde
  exit 0
fi
which kde
if [ $? -eq 0 ]; then
  kde
  exit 0
fi

# gnome
which gnome-session
if [ $? -eq 0 ]; then
  gnome-session
  exit 0
fi

# blackbox
#if [ "'which blackbox'" != "" ]; then
#  blackbox
#  exit 0
#fi

# fvwm95
#if [ "'which fvwm95'" != "" ]; then
#  fvwm95
#  exit 0
#fi

# fall back on xterm
which xterm
if [ $? -eq 0 ]; then
  xterm
  exit 0
fi
