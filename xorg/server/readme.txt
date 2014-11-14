
------------------------------------------------------
11/01/2014
------------------------------------------------------

There are four modules built for the Xorg driver model and one configuration file
This works best with newer Xorg installs, xserver 1.10 +, example Debian 7 +, Ubuntu 12.04 +

To see what version you have, run
xdpyinfo | grep version:
or
dpkg -l | grep xserver-xorg-core
or
yum list | grep xorg-x11-server-Xorg

It should compile with older version and may run but may be problems.
Usually, the problems are related to startup / login.

autotools should build and install them

./bootstrap
./configure
make
sudo make install

This should install the following...

libxorgxrdp.so goes in /usr/lib/xorg/modules/
xrdpdev_drv.so goes in /usr/lib/xorg/modules/drivers/
xrdpkeyb_drv.so goes in /usr/lib/xorg/modules/input/
xrdpmouse_drv.so goes in /usr/lib/xorg/modules/input/
xorg.conf goes in /etc/X11/xrdp/

with all these components in place, you can start Xorg with the xrdp modules with
Xorg -config xrdp/xorg.conf -logfile /tmp/Xtmp.log -noreset -ac :10
or
Xorg -config xrdp/xorg.conf -logfile /dev/null -noreset -ac :10




older notes

------------------------------------------------------
 Notes for building xrdpdev_drv.so and libxorgxrdp.so
------------------------------------------------------

Pre-requisites:
    o sudo apt-get install xserver-xorg-dev

quick and easy way to build and run the driver
    o cd xorg/server
    o ./test-in-home.sh

    o see /etc/X11/xrdp/xorg.conf to see how things are configured

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
