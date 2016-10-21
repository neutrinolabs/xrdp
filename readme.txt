
xrdp 0.9.0

Credits
  This project is very much dependent on NeutrinoRDP, FreeRDP, rdesktop, and
  the work of Matt Chapman and the NeutrinoRDP team members, of which I'm a
  member.

  Mark from up 19.9 was the first to work with rdp server code.

Tested with linux on i386, x64, sparc, and ppc.
I've got it compiling and working in windows with Borland free tools.
None of the sesman or Xserver stuff works in windows of course.

xrdp directory is the main server code
vnc directory is a simple vnc client module for xrdp
sesman directory is a session manager for xrdp that uses Xvnc for the Xserver
libxrdp directory is a static library needed by xrdp
rdp is an rdp client module for connecting to another rdp server
xup is a module used to connect to an rdp specific X11 server
Xserver is the files needed to build an rdp specific X11 server
COPYING is the license file
design.txt is an attempt to explain the project design
coding_style.md describes the coding style for the project

since version 0.5.0 we switch to autotools to build xrdp

to build and install

change to the xrdp directory and run
./bootstrap
./configure
make
then as root
make install


Customize Desktop Environment for xRDP Session

If you do not want to use the default desktop environment, you can customize it by creating a .Xclients file (X is capital!!!) in your home directory to launch the desktop environment you want and making it executable. In order to do this, open a terminal and run one of the following commands

Gnome 3:
echo "gnome-session" > ~/.Xclients
chmod +x ~/.Xclients
sudo systemctl restart xrdp.service

Gnome Fallback:
echo "gnome-fallback" > ~/.Xclients
chmod +x ~/.Xclients
sudo systemctl restart xrdp.service

KDE:
echo "startkde" > ~/.Xclients
chmod +x ~/.Xclients
sudo systemctl restart xrdp.service

MATE:
echo "mate-session" > ~/.Xclients
chmod +x ~/.Xclients
sudo systemctl restart xrdp.service

Cinnamon:
echo "cinnamon" > ~/.Xclients
chmod +x ~/.Xclients
sudo systemctl restart xrdp.service

Xfce4:
echo "startxfce4" > ~/.Xclients
chmod +x ~/.Xclients
sudo systemctl restart xrdp.service

if you're not using systemd, try sudo service xrdp restart

see file-loc.txt to see what files are installed where

Jay Sorg
