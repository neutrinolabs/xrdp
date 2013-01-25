#!/bin/sh

# buildx.sh: a script for building X11R7.6 X server for use with xrdp
#
# Copyright 2011-2012 Jay Sorg Jay.Sorg@gmail.com
#
# Authors
#       Jay Sorg Jay.Sorg@gmail.com
#       Laxmikant Rashinkar LK.Rashinkar@gmail.com
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# debian packages needed
# flex bison libxml2-dev intltool xsltproc xutils-dev python-libxml2 g++ xutils

download_file()
{
    local file url status
    file=$1

    # if we already have the file, don't download it
    if [ -r downloads/$file ]; then
        return 0
    fi

    echo "downloading file $file"
    case "$file" in
    pixman-0.15.20.tar.bz2)
        url=http://ftp.x.org/pub/individual/lib/$file ;;
    libdrm-2.4.26.tar.bz2)
        url=http://dri.freedesktop.org/libdrm/$file ;;
    MesaLib-7.10.3.tar.bz2)
        url=ftp://ftp.freedesktop.org/pub/mesa/7.10.3/$file ;;
    freetype-2.4.6.tar.bz2)
        url=http://download.savannah.gnu.org/releases/freetype/$file ;;
    xkeyboard-config-2.0.tar.bz2)
        url=http://www.x.org/releases/individual/data/xkeyboard-config/$file ;;
    makedepend-1.0.3.tar.bz2)
        url=http://xorg.freedesktop.org/releases/individual/util/$file ;;
    libxml2-sources-2.7.8.tar.gz)
        url=ftp://ftp.xmlsoft.org/libxml2/$file ;;
    Python-2.5.tar.bz2)
        url=http://www.python.org/ftp/python/2.5/$file ;;
    Python-2.7.tar.bz2)
        url=http://www.python.org/ftp/python/2.7/$file ;;
    expat-2.0.1.tar.gz)
        url=http://server1.xrdp.org/xrdp/$file ;;
    cairo-1.8.8.tar.gz)
        url=http://server1.xrdp.org/xrdp/$file ;;
    libpng-1.2.46.tar.gz)
        url=http://server1.xrdp.org/xrdp/$file ;;
    intltool-0.41.1.tar.gz)
        url=http://launchpad.net/intltool/trunk/0.41.1/+download/$file ;;
    libxslt-1.1.26.tar.gz)
        url=ftp://xmlsoft.org/libxslt/$file ;;
    fontconfig-2.8.0.tar.gz)
        url=http://server1.xrdp.org/xrdp/$file ;;
    *)
        url=$download_url/$file ;;
    esac
    cd downloads
    wget -cq "url"
    status=$?
    cd ..
    return $status
}

remove_modules()
{
    if [ -d cookies ]; then
        rm cookies/*
    fi

    if [ ! -d build_dir ]; then
        echo ""
        echo "build_dir does not exist; nothing to delete"
        echo ""
        exit 0
    fi

    cd build_dir

    while IFS=: read mod_file mod_dir mod_args
    do
        if [ -d $mod_dir ]; then
            rm -rf $mod_dir
        fi
    done < ../$data_file

    cd ..
}

extract_it()
{
    mod_file=$1
    mod_name=$2
    mod_args=$3

    if [ -e cookies/$mod_name.extracted ]; then
        return 0
    fi

    # download file
    if ! download_file $mod_file
    then
        echo ""
        echo "failed to download $mod_file - aborting build"
        echo ""
        exit 1
    fi

    cd build_dir

    # if pkg has not yet been extracted, do so now
    if [ ! -d $mod_name ]; then
        case "$mod_file" in
        *.tar.bz2) comp=j ;;
        *.tar.gz) comp=z ;;
        *.tar.xz) comp=J ;;
        *.tar) comp= ;;
        *) echo "unknown compressed module $mod_name" ; exit 1 ;;
        esac
        if ! tar x${comp}f ../downloads/$mod_file > /dev/null
        then
            echo "error extracting module $mod_name"
            exit 1
        fi
    fi

    # patch and configure module - we only need to do this once
    cd $mod_name
    # check for patches
    if [ -e ../../$mod_name.patch ]; then
        patch -p1 < ../../$mod_name.patch
    fi
    # now configure
    echo "executing ./configure --prefix=$PREFIX_DIR $mod_args"
    if ! ./configure --prefix=$PREFIX_DIR $mod_args
    then
        echo "configuration failed for module $mod_name"
        exit 1
    fi

    cd ../..

    touch cookies/$mod_name.extracted
}

make_it()
{
    mod_file=$1
    mod_name=$2
    mod_args=$3

    count=`expr $count + 1`

    # if a cookie with $mod_name exists...
    if [ -e cookies/$mod_name.installed ]; then
        # ...package has already been installed
        return 0
    fi

    echo ""
    echo "*** processing module $mod_name ($count of $num_modules) ***"
    echo ""

    if ! extract_it $mod_file $mod_name "$mod_args"
    then
        echo ""
        echo "extract failed for module $mod_name"
        echo ""
        exit 1
    fi

    # make module
    if [ ! -e cookies/$mod_name.made ]; then
        if ! (cd build_dir/$mod_name ; make)
        then
            echo ""
            echo "make failed for module $mod_name"
            echo ""
            exit 1
        fi
        touch cookies/$mod_name.made
    fi

    # install module
    if ! (cd build_dir/$mod_name ; make install)
    then
        echo ""
        echo "make install failed for module $mod_name"
        echo ""
        exit 1
    fi

    # special case after installing python make this sym link
    # so Mesa builds using this python version
    case "$mod_name" in
    *Python-2*)
        (cd build_dir/$mod_name ; ln -s python $PREFIX_DIR/bin/python2)
        ;;
    esac

    touch cookies/$mod_name.installed
    return 0
}

# this is where we store list of modules to be processed
data_file=x11_file_list.txt

# this is the default download location for most modules
download_url=http://www.x.org/releases/X11R7.6/src/everything

num_modules=`cat $data_file | wc -l`
count=0

##########################
# program flow starts here
##########################

if [ $# -lt 1 ]; then
    echo ""
    echo "usage: buildx.sh <installation dir>"
    echo "usage: buildx.sh <clean>"
    echo "usage: buildx.sh default"
    echo "usage: buildx.sh <installation dir> drop - set env and run bash in rdp dir"
    echo ""
    exit 1
fi

# remove all modules
if [ "$1" = "clean" ]; then
    echo "removing source modules"
    remove_modules
    exit 0
fi

if [ "$1" = "default" ]; then
    export PREFIX_DIR=$PWD/staging
else
    export PREFIX_DIR=$1
fi

if ! test -d $PREFIX_DIR; then
    echo "dir does not exist, creating [$PREFIX_DIR]"
    if ! mkdir $PREFIX_DIR
    then
        echo "mkdir failed [$PREFIX_DIR]"
        exit 0
    fi
fi

echo "using $PREFIX_DIR"

export PKG_CONFIG_PATH=$PREFIX_DIR/lib/pkgconfig:$PREFIX_DIR/share/pkgconfig
export PATH=$PREFIX_DIR/bin:$PATH
export LDFLAGS=-Wl,-rpath=$PREFIX_DIR/lib
export CFLAGS="-I$PREFIX_DIR/include -fPIC -O2"

# prefix dir must exist...
if [ ! -d $PREFIX_DIR ]; then
    if ! mkdir -p $PREFIX_DIR
    then
        echo "$PREFIX_DIR does not exist; failed to create it - cannot continue"
        exit 1
    fi
fi

# ...and be writable
if [ ! -w $PREFIX_DIR ]; then
    echo "directory $PREFIX_DIR is not writable - cannot continue"
    exit 1
fi

# create a downloads dir
if [ ! -d downloads ]; then
    if ! mkdir downloads
    then
        echo "error creating downloads directory"
        exit 1
    fi
fi

# this is where we do the actual build
if [ ! -d build_dir ]; then
    if ! mkdir build_dir
    then
        echo "error creating build_dir directory"
        exit 1
    fi
fi

# this is where we store cookie files
if [ ! -d cookies ]; then
    if ! mkdir cookies
    then
        echo "error creating cookies directory"
        exit 1
    fi
fi

while IFS=: read mod_file mod_dir mod_args
do
    mod_args=`eval echo $mod_args`

    make_it $mod_file $mod_dir "$mod_args"
done < $data_file

echo "build for X OK"

X11RDPBASE=$PREFIX_DIR
export X11RDPBASE

cd rdp
if ! make
then
    echo "error building rdp"
    exit 1
fi

# this will copy the build X server with the other X server binaries
strip X11rdp
cp X11rdp $X11RDPBASE/bin

if [ "$2" = "drop" ]; then
    echo ""
    echo "dropping you in dir, type exit to get out"
    bash
    exit 1
fi

echo "All done"
