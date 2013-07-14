
Notes for building xrdpdev_drv.so and libxorgxrdp.so



to run it
create /etc/X11/xrdp
copy xorg.conf into it

copy xrdpdev_drv.so to /usr/lib/xorg/modules/drivers
copy libxorgxrdp.so to /usr/lib/xorg/modules

strat xserver like this
Xorg -modulepath /usr/lib/xorg/modules -config xrdp/xorg.conf -logfile /tmp/Xjay.log -novtswitch -sharevts -noreset -nohwaccess -ac :10

