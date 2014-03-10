#!/bin/sh

if [ -e /etc/X11/xrdp/xorg.conf ]; then
    echo "/etc/X11/xrdp/xorg.conf ok"
else
    echo "/etc/X11/xrdp/xorg.conf missing, run"
    echo "sudo mkdir /etc/X11/xrdp"
    echo "sudo cp xrdpdev/xorg.conf /etc/X11/xrdp/"
    exit 1
fi

if [ -d $HOME/xorg-modules ]; then
    echo "found directory ok"
else
    echo "creating directory"
    mkdir $HOME/xorg-modules
    mkdir $HOME/xorg-modules/drivers
    mkdir $HOME/xorg-modules/extensions
    mkdir $HOME/xorg-modules/input

    cp /usr/lib/xorg/modules/libfb.so $HOME/xorg-modules/
    cp /usr/lib/xorg/modules/libint10.so $HOME/xorg-modules/
    cp /usr/lib/xorg/modules/libvbe.so $HOME/xorg-modules/

    cp /usr/lib/xorg/modules/extensions/libdbe.so $HOME/xorg-modules/extensions/
    cp /usr/lib/xorg/modules/extensions/libdri.so $HOME/xorg-modules/extensions/
    cp /usr/lib/xorg/modules/extensions/libdri2.so $HOME/xorg-modules/extensions/
    cp /usr/lib/xorg/modules/extensions/libextmod.so $HOME/xorg-modules/extensions/
    cp /usr/lib/xorg/modules/extensions/libglx.so $HOME/xorg-modules/extensions/
    cp /usr/lib/xorg/modules/extensions/librecord.so $HOME/xorg-modules/extensions/

fi

make
if test $? -ne 0
then
    echo "make failed"
    exit 1
fi
make xinstall
exec Xorg -modulepath $HOME/xorg-modules -config xrdp/xorg.conf -logfile /tmp/Xtmp.log -novtswitch -sharevts -noreset -ac vt7 :20
