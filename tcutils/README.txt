A QT based utility program for thin clients using xrdp and NeutrinoRDP

This program sends commands to NeutrinoRDP to do something
useful on the client end (such as unmounting a USB drive,
or powering down the client)

Required packages to build tcutils:
-----------------------------------
libqt4-gui
qt4-dev-tools

to build tcutils:
-----------------
qmake
make

To run tcutils:
---------------
include xrdpapi/.libs in your LD_LIBRARY_PATH

Example:
--------
export LD_LIBRARY_PATH=../xrdpapi/.libs
run tcutils inside the xfreerdp session

this is how we run xfreerdp:
----------------------------
./xfreerdp --sec rdp --plugin tcutils 192.168.2.149

