#!/bin/sh
if [ "'which startkde'" != "" ]; then
  startkde
  exit 0
fi
if [ "'which kde'" != "" ]; then
  kde
  exit 0
fi
if [ "'which blackbox'" != "" ]; then
  blackbox
  exit 0
fi
if [ "'which fvwm95'" != "" ]; then
  fvwm95
  exit 0
fi
if [ "'which xterm'" != "" ]; then
  xterm
  exit 0
fi
