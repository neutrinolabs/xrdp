#!/bin/sh
#
# all directories can be read only except
# Read Write
#  share/X11/xkb/compiled/

if test $# -lt 1
then
    echo ""
    echo "usage: clean_build_dir.sh <installation dir>"
    echo ""
    exit 1
fi

BASEDIR=$1

if ! test -d $BASEDIR
then
    echo "error directory $BASEDIR does not exist"
    exit 1
fi

if ! test -w $BASEDIR
then
    echo "error directory $BASEDIR is not writable"
    exit 1
fi

echo cleaning $BASEDIR

if ! test -x $BASEDIR/bin/X11rdp
then
    echo "error $BASEDIR/bin/X11rdp does not exist"
fi

bin_check_file()
{
    if [ "$1" = "X11rdp" ]
    then
        return 0
    fi
    if [ "$1" = "xkbcomp" ]
    then
        return 0
    fi
    rm -f $1
    return 0
}

HOLDPATH=$PWD
cd $BASEDIR

# remove unused directories
rm -fr man/
rm -fr include/
rm -fr lib/python2.7/
rm -fr lib/pkgconfig/
rm -fr share/pkgconfig/
rm -fr share/gtk-doc
rm -fr share/doc
rm -fr share/man
rm -fr share/aclocal
rm -fr share/intltool
rm -fr share/util-macros

# remove development files
rm -f lib/*.a
rm -f lib/*.la
rm -f lib/xorg/modules/*.a
rm -f lib/xorg/modules/*.la

# remove symbols
#strip lib/*.so
#strip lib/xorg/modules/*.so

# remove hardware specific files
rm -f lib/dri/i915_dri.so
rm -f lib/dri/i965_dri.so
rm -f lib/dri/mach64_dri.so
rm -f lib/dri/mga_dri.so
rm -f lib/dri/r128_dri.so
rm -f lib/dri/r200_dri.so
rm -f lib/dri/r300_dri.so
rm -f lib/dri/r600_dri.so
rm -f lib/dri/radeon_dri.so
rm -f lib/dri/savage_dri.so
#strip lib/dri/swrast_dri.so
rm -f lib/dri/tdfx_dri.so
rm -f lib/dri/unichrome_dri.so

# remove extra bin tools
cd bin
for i in *
do
    if ! test -d "$i"
    then
        bin_check_file $i
    fi
done
cd ..

cd $HOLDPATH
