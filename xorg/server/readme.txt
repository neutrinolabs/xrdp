
Notes for building xrdpdev_drv.so and libxorgxrdp.so



to run it
create /etc/X11/xrdp
copy xorg.conf into it

Xorg -config xrdp/xorg.conf -logfile /tmp/Xjay.log -novtswitch -sharevts -noreset :10

