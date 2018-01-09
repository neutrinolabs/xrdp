The latest version of this document can be found at wiki.

* https://github.com/neutrinolabs/xrdp/wiki/How-to-set-up-audio-redirection


# Overview
xrdp supports audio redirection using PulseAudio, which is a sound system for
POSIX operating systems. Server to client redirection is compliant to Remote
Desktop Procol standard [[MS-RDPEA]](https://msdn.microsoft.com/en-us/library/cc240933.aspx)
but client to server redirection implementation is proprietary. Accordingly,
server to client redirection is available with many of RDP clients including
Microsoft client but client to server redirection requires NeutrinoRDP client,
not available with other clients.

Here is how to build pulseaudio modules for your distro, so you can have audio
support through xrdp.

# Prerequisites
Prepare xrdp source in your home directory. Of course, you can choose another
directory.

    cd ~
    git clone https://github.com/neutrinolabs/xrdp.git

In this instruction, pulseaudio version is **10.0**. Replace the version number
in this instruction if your environment has different versions. You can find
out your pulseaudio version executing the following command:

    pulseaudio --version

# How to build

## Debian 9 / Ubuntu

This instruction also should be applicable to Ubuntu family.

### Prerequisites

Some build tools and package development tools are required. Make sure install
the tools.

    apt install build-essential dpkg-dev

### Prepare & build

Install pulseaudio and requisite packages to build pulseaudio.

    apt install pulseaudio
    apt build-dep pulseaudio

Fetch the pulseaudio source . You'll see `pulseaudio-10.0` directory in your
current directory.

    apt source pulseaudio

Enter into the directory and build the pulseaudio package.

    cd pulseaudio-10.0
    ./configure

Finally, let's make. You'll have two .so files `module-xrdp-sink.so` and
`module-xrdp-source.so`.

    cd ~/xrdp/sesman/chansrv/pulse
    make PULSE_DIR="~/pulseaudio-10.0"

## Other distro

First off, find out your pulseaudio version using `pulseaudio --version`
command. Download the tarball of the pulseaudio version that you have.

* https://freedesktop.org/software/pulseaudio/releases/

After downloading the tarball, extact the tarball and `cd` into the source
directory, then run `./configure`.

    wget https://freedesktop.org/software/pulseaudio/releases/pulseaudio-10.0.tar.xz
    tar xf pulseaudio-10.0.tar.gz
    cd pulseaudio-10.0
    ./configure

If additional packages are required to run `./configure`, install requisite
packages depending on your environment.

Finally, let's make. You'll have two .so files `module-xrdp-sink.so` and
`module-xrdp-source.so`.

    cd ~/xrdp/sesman/chansrv/pulse
    make PULSE_DIR="~/pulseaudio-10.0"

# Install

Install process is not distro specific except for install destination. Install
built two .so files into the pulseaudio modules directory. Typically,
`/usr/lib/pulse-10.0/modules` for Debian, `/usr/lib64/pulse-10.0/modules` for
CentOS 7. Other distro might have different path. Find out the right path for
your distro.

Look into the directory with `ls` command. You'll see lots of `module-*.so`
files. There's the place!

    cd ~/xrdp/sesman/chansrv/pulse
    for f in *.so; do install -s -m 644 $f /usr/lib/pulse-10.0/modules; done

This command is equivalent to following:

    install -s -m 644 module-xrdp-sink.so /usr/lib/pulse-10.0/modules
    install -s -m 644 module-xrdp-source.so /usr/lib/pulse-10.0/modules

Well done! Pulseaudio modules should be properly built and installed.


# See if it works

To see if it works, run `pavumeter` in the xrdp session.  Playback any YouTube
video in Firefox. You'll see "Showing signal levels of **xrdp sink**" and
volume meter moving.
