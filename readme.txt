
Credits
  This project is very much dependent on rdesktop and the work of Matt Chapman
  and the rdesktop team members, of which I'm a member

  Mark from up 19.9 was the first to work with rdp server code.

To use this run make the ./xrdp, then connect with rdesktop or mstsc.
Tested with linux and i386.
I've got it compiling and working in windows with borland free tools.
I've got it compiling and working with linux on x64 processor

xrdp directory is the main server code
vnc directory is a simple vnc client module for xrdp
sesman directory is a session manager for xrdp that uses Xvnc for the Xserver
libxrdp directory is a static library needed by xrdp
rdp is an rdp client module for connecting to another rdp server
xup is a module used to connect to an rdp specific X11 server
Xserver is the files needed to build an rdp specific X11 server
COPYING is the licence file
design.txt is an attempt to expain the project design
prog_std.txt is an attemp to explain the programming standard used

run make in main xrdp directory to build
run make install as root to install in /usr/local/xrdp

Jay
