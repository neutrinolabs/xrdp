
Notes for building xrdpdev_drv.so and libxorgxrdp.so



to run it
create /etc/X11/xrdp
copy xorg.conf into it

copy xrdpdev_drv.so to /usr/lib/xorg/modules/drivers
copy libxorgxrdp.so to /usr/lib/xorg/modules

copy xrdpmouse_drv.so to /usr/lib/xorg/modules/input
copy xrdpkeyb_drv.so to /usr/lib/xorg/modules/input

start xserver like this
Xorg -modulepath /usr/lib/xorg/modules -config xrdp/xorg.conf -logfile /tmp/Xjay.log -novtswitch -sharevts -noreset -nohwaccess -ac :10
or this on older Xorg but need /dev/vc/ thing below
Xorg -modulepath /home/jay/xorg-modules -config xrdp/xorg.conf -logfile /tmp/Xjay.log -novtswitch -sharevts -noreset -ac vt7 :10

older Xorg don't have -nohwaccess so you need to run Xorg as root
or do something like this.

sudo rm /dev/tty0
sudo mknod -m 666 /dev/tty0 c 4 0

sudo mkdir /dev/vc/
sudo mknod -m 666 /dev/vc/7 c 7 7

--modules
    libfb.so
    libint10.so
    libvbe.so
    libxorgxrdp.so
----drivers
      xrdpdev_drv.so
----extensions
      libdbe.so
      libdri.so
      libdri2.so
      libextmod.so
      libglx.so
      librecord.so
----input
      xrdpkeyb_drv.so
      xrdpmouse_drv.so

dpkg -S /usr/lib/xorg/modules/extensions/libglx.so
xserver-xorg-core
