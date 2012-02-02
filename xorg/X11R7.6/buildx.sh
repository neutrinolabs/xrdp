#!/bin/sh

# build.sh: a script for building X11R7.6 X server for use with xrdp #
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

# flex bison libxml2-dev intltool
# xsltproc

download_file()
{
    file=$1

    cd downloads

    echo "downloading file $file"
    if [ "$file" = "pixman-0.15.20.tar.bz2" ]; then
        wget -cq http://ftp.x.org/pub/individual/lib/$file
        status=$?
        cd ..
        return $status
    elif [ "$file" = "libdrm-2.4.26.tar.bz2" ]; then
        wget -cq http://dri.freedesktop.org/libdrm/$file
        status=$?
        cd ..
        return $status
    elif [ "$file" = "MesaLib-7.10.3.tar.bz2" ]; then
        wget -cq ftp://ftp.freedesktop.org/pub/mesa/7.10.3/$file
        status=$?
        cd ..
        return $status
    elif [ "$file" = "expat-2.0.1.tar.gz" ]; then
        wget -cq http://surfnet.dl.sourceforge.net/project/expat/expat/2.0.1/expat-2.0.1.tar.gz
        status=$?
        cd ..
        return $status
    elif [ "$file" = "freetype-2.4.6.tar.bz2" ]; then
        wget -cq http://download.savannah.gnu.org/releases/freetype/freetype-2.4.6.tar.bz2
        status=$?
        cd ..
        return $status
    elif [ "$file" = "xkeyboard-config-2.0.tar.bz2" ]; then
        wget -cq http://www.x.org/releases/individual/data/xkeyboard-config/xkeyboard-config-2.0.tar.bz2
        status=$?
        cd ..
        return $status
    elif [ "$file" = "makedepend-1.0.3.tar.bz2" ]; then
        wget -cq http://xorg.freedesktop.org/releases/individual/util/makedepend-1.0.3.tar.bz2
        status=$?
        cd ..
        return $status
    elif [ "$file" = "libxml2-sources-2.7.8.tar.gz" ]; then
        wget -cq ftp://ftp.xmlsoft.org/libxml2/libxml2-sources-2.7.8.tar.gz
        status=$?
        cd ..
        return $status
    elif [ "$file" = "Python-2.5.tar.bz2" ]; then
        wget -cq http://www.python.org/ftp/python/2.5/Python-2.5.tar.bz2
        status=$?
        cd ..
        return $status
    elif [ "$file" = "Python-2.7.tar.bz2" ]; then
        wget -cq http://www.python.org/ftp/python/2.7/Python-2.7.tar.bz2
        status=$?
        cd ..
        return $status
    elif [ "$file" = "expat-2.0.1.tar.gz" ]; then
        wget -cq http://server1.xrdp.org/xrdp/expat-2.0.1.tar.gz
        status=$?
        cd ..
        return $status
    elif [ "$file" = "cairo-1.8.8.tar.gz" ]; then
        wget -cq http://server1.xrdp.org/xrdp/cairo-1.8.8.tar.gz
        status=$?
        cd ..
        return $status
    elif [ "$file" = "libpng-1.2.46.tar.gz" ]; then
        wget -cq http://server1.xrdp.org/xrdp/libpng-1.2.46.tar.gz
        status=$?
        cd ..
        return $status
    elif [ "$file" = "intltool-0.41.1.tar.gz" ]; then
        wget -cq http://launchpad.net/intltool/trunk/0.41.1/+download/intltool-0.41.1.tar.gz
        status=$?
        cd ..
        return $status
    elif [ "$file" = "libxslt-1.1.26.tar.gz" ]; then
        wget -cq ftp://xmlsoft.org/libxslt/libxslt-1.1.26.tar.gz
        status=$?
        cd ..
        return $status
    elif [ "$file" = "fontconfig-2.8.0.tar.gz" ]; then
        wget -cq http://server1.xrdp.org/xrdp/fontconfig-2.8.0.tar.gz
        status=$?
        cd ..
        return $status
    else
        wget -cq $download_url/$file
        status=$?
        cd ..
        return $status
    fi
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

    while read line
    do
        mod_dir=`echo $line | cut -d':' -f2`
        if [ -d $mod_dir ]; then
            rm -rf $mod_dir
        fi
    done < ../$data_file

    cd ..
}

make_it()
{
    mod_file=$1
    mod_name=$2
    mod_args=$3

    count=`expr $count + 1`

    # if a cookie with $mod_name exists...
    if [ -e cookies/$mod_name ]; then
        # ...package has already been built
        return 0
    fi

    echo ""
    echo "*** processing module $mod_name ($count of $num_modules) ***"
    echo ""

    # download file
    download_file $mod_file
    if [ $? -ne 0 ]; then
        echo ""
        echo "failed to download $mod_file - aborting build"
        echo ""
        exit 1
    fi

    cd build_dir

    # if pkg has not yet been extracted, do so now
    if [ ! -d $mod_name ]; then
        echo $mod_file | grep -q tar.bz2
        if [ $? -eq 0 ]; then
            tar xjf ../downloads/$mod_file > /dev/null 2>&1
        else
            tar xzf ../downloads/$mod_file > /dev/null 2>&1
        fi
        if [ $? -ne 0 ]; then
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
    ./configure --prefix=$PREFIX_DIR $mod_args
    if [ $? -ne 0 ]; then
        echo "configuration failed for module $mn"
        exit 1
    fi

    # make module
    make
    if [ $? -ne 0 ]; then
        echo ""
        echo "make failed for module $mod_name"
        echo ""
        exit 1
    fi

    # install module
    make install
    if [ $? -ne 0 ]; then
        echo ""
        echo "make install failed for module $mod_name"
        echo ""
        exit 1
    fi

    # special case after installing python make this sym link
    # so Mesa builds using this python version
    case "$mod_name" in
      *Python-2*)
      ln -s python $PREFIX_DIR/bin/python2
      ;;
    esac

    cd ../..
    touch cookies/$mod_name
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
    echo "usage: build.sh <installation dir>"
    echo "usage: build.sh <clean>"
    echo "usage: build.sh default"
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
    echo "dir does not exit, creating [$PREFIX_DIR]"
    mkdir $PREFIX_DIR
    if ! test $? -eq 0; then
        echo "mkdir failed [$PREFIX_DIR]"
        exit 0
    fi
fi

echo "using $PREFIX_DIR"

export PKG_CONFIG_PATH=$PREFIX_DIR/lib/pkgconfig:$PREFIX_DIR/share/pkgconfig
export PATH=$PREFIX_DIR/bin:$PATH
export LD_LIBRARY_PATH=$PREFIX_DIR/lib
# really only needed for x84
export CFLAGS=-fPIC

# prefix dir must exist...
if [ ! -d $PREFIX_DIR ]; then
    mkdir -p $PREFIX_DIR
    if [ $? -ne 0 ]; then
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
    mkdir downloads
    if [ $? -ne 0 ]; then
        echo "error creating downloads directory"
        exit 1
    fi
fi

# this is where we do the actual build
if [ ! -d build_dir ]; then
    mkdir build_dir
    if [ $? -ne 0 ]; then
        echo "error creating build_dir directory"
        exit 1
    fi
fi

# this is where we store cookie files
if [ ! -d cookies ]; then
    mkdir cookies
    if [ $? -ne 0 ]; then
        echo "error creating cookies directory"
        exit 1
    fi
fi

while read line
do
    mod_file=`echo $line | cut -d':' -f1`
    mod_dir=`echo $line | cut -d':' -f2`
    mod_args=`echo $line | cut -d':' -f3`
    mod_args=`eval echo $mod_args`

    make_it $mod_file $mod_dir "$mod_args"

done < $data_file

echo "All done"
