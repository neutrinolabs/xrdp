A QT based media player that runs on a RDP server and
redirects audio/video to the client where it is decoded
and rendered locally

Required packages to build vrplayer:
------------------------------------
libqt4-gui
qt4-dev-tools
libavutil-dev
libavformat-dev

to build vrplayer
-----------------
cd ../xrdpvr
make
cd ..
qmake
make

To run vrplayer
---------------
include xrdpapi/.libs and xrdpvr/.libs in your LD_LIBRARY_PATH

Example:
--------
export LD_LIBRARY_PATH=../xrdpapi/.libs:../xrdpvr/.libs
run vrplayer inside the xfreerdp session

this is how we run xfreerdp:
----------------------------
./xfreerdp --sec rdp --plugin xrdpvr 192.168.2.149

