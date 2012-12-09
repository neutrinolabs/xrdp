
A QT based media player that runs on a RDP server and 
redirects audio/video to the client where it is decoded 
and rendered locally

To build vrplayer, installl QT 4.x , then run

qmake
make

To run vrplayer, include xrdpapi/.libs and xrdpvr/.libs in 
your LD_LIBRARY_PATH

