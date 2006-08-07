This is the debian directory used to make a .deb file to install
xrdp.  It installs to /usr/lib/xrdp.

To make a new deb type
remember, debian/rules must be executable
fakeroot dpkg-buildpackage -b -uc -us -d

Jay
