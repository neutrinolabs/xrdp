#!/bin/bash

# receives version, release number and source directory as arguments

VERSION=$1
RELEASE=$2
SRCDIR=$3
PKGDEST=$4

PACKDIR=x11rdp-files
DESTDIR=$PACKDIR/opt
NAME=x11rdp
ARCH=$( dpkg --print-architecture )


sed -i -e  "s/DUMMYVERINFO/$VERSION-$RELEASE/"  $PACKDIR/DEBIAN/control
sed -i -e  "s/DUMMYARCHINFO/$ARCH/"  $PACKDIR/DEBIAN/control
# need a different delimiter, since it has a path
sed -i -e  "s,DUMMYDIRINFO,$SRCDIR,"  $PACKDIR/DEBIAN/postinst

mkdir -p $DESTDIR
cp -Rf $SRCDIR $DESTDIR
dpkg-deb --build $PACKDIR $PKGDEST/${NAME}_$VERSION-${RELEASE}_${ARCH}.deb

# revert to initial state
rm -rf $DESTDIR
sed -i -e  "s/$VERSION-$RELEASE/DUMMYVERINFO/"  $PACKDIR/DEBIAN/control
sed -i -e  "s/$ARCH/DUMMYARCHINFO/"  $PACKDIR/DEBIAN/control
# need a different delimiter, since it has a path
sed -i -e  "s,$SRCDIR,DUMMYDIRINFO,"  $PACKDIR/DEBIAN/postinst
