
readme for X11rdp server.

Directions for Xfree 4.5, 4.6 and Xorg 6.8.2, 6.9.0.

make sure these are installed.
gcc, make, bison, flex, g++, ncursors, libpng-dev, expat-dev,
freetype-dev

First run make World.
This is the most dificult part.

copy makefile_x11rdp to /xc/programs/Xserver
copy /hw/rdp directory to /xc/programs/Xserver

run make -f makefile_x11rdp

Jay