#!/bin/sh

# Script to install a version of cppcheck in ~/cppcheck.local/
#
# Used by Travis-CI builds, until Travis supports cppcheck natively
#
# Currently only supports git repos as sources
#
# Usage: /path/to/install_cppcheck.sh <cppcheck-git-repo> <version-tag>

INSTALL_ROOT=~/cppcheck.local

# ----------------------------------------------------------------------------
# U S A G E
# ----------------------------------------------------------------------------
usage()
{
    echo "** Usage: $0 <git-repo URL> <version-tag>"
    echo "   e.g. $0 https://github.com/danmar/cppcheck.git 1.90"
} >&2

# ----------------------------------------------------------------------------
# C A L L _ M A K E
#
# Calls make with the specified parameters, but only displays the error
# log if it fails
# ----------------------------------------------------------------------------
call_make()
{
    # Disable set -e, if active
    set_entry_opts=`set +o`
    set +e

    status=1
    log=`mktemp /tmp/cppcheck-log.XXXXXXXXXX`
    if [ -n "$log" ]; then
        make "$@" >"$log" 2>&1
        status=$?
        if [ $status -ne 0 ]; then
            cat "$log" >&2
        fi
        rm "$log"
    fi

    # Re-enable `set -e` if active before
    $set_entry_opts

    return $status
}


# ----------------------------------------------------------------------------
# C R E A T E   Z 3   V E R S I O N   H
#
# Older versions of libz3-dev do not come packaged with z3_version.h. This
# function uses the z3 command to create a copy of this file in the
# cppcheck i#externalsi# directory.
# ----------------------------------------------------------------------------
create_z3_version_h()
{
    set -- `z3 --version`
    if [ $# != 3 -o "$1/$2" != Z3/version ]; then
        echo "** Unexpected output from z3 command '$*'" >&2
        false
    else
        z3ver=$3 ; # e.g. 4.4.3
        set -- `echo $z3ver | tr '.' ' '`
        if [ $# != 3 ]; then
            echo "** Unable to determine Z3 version from '$z3ver'" >&2
            false
        else
            {
                echo "#ifndef Z3_MAJOR_VERSION"
                echo "#define Z3_MAJOR_VERSION $1"
                echo "#endif"
                echo
                echo "#ifndef Z3_MINOR_VERSION"
                echo "#define Z3_MINOR_VERSION $2"
                echo "#endif"
                echo
                echo "#ifndef Z3_BUILD_NUMBER"
                echo "#define Z3_BUILD_NUMBER $3"
                echo "#endif"
            } >externals/z3_version.h
            echo "  - Created z3_version.h for $1.$2.$3"
        fi
    fi
}

# ----------------------------------------------------------------------------
# M A I N
# ----------------------------------------------------------------------------
if [ $# -ne 2 ]; then
    usage
    exit 1
fi
REPO_URL="$1"
CPPCHECK_VER="$2"

# Already installed?
exe=$INSTALL_ROOT/$CPPCHECK_VER/bin/cppcheck
if [ -x "$exe" ]; then
    echo "cppcheck version $CPPCHECK_VER is already installed at $exe" >&2
    exit 0
fi

workdir=`mktemp -d /tmp/cppcheck.XXXXXXXXXX`
if [ -z "$workdir" ]; then
    echo "** Unable to create temporary working directory" 2>&1
    exit 1
fi

# Use a sub-process for the next bit to restrict the scope of 'set -e'
(
    set -e ; # Exit sub-process on first error

    # Put everything in this directory
    FILESDIR=$INSTALL_ROOT/$CPPCHECK_VER

    # See https://stackoverflow.com/questions/
    #     791959/download-a-specific-tag-with-git
    git clone -b "$CPPCHECK_VER" --depth 1 "$REPO_URL" "$workdir"

    cd "$workdir"

    make_args="MATCHCOMPILER=yes FILESDIR=$FILESDIR PREFIX=$FILESDIR"

    case "$CPPCHECK_VER" in
        1.*)
            # CFGDIR is needed for cppcheck before 1.86
            make_args="$make_args CFGDIR=$FILESDIR"
            ;;
        2.8 | 2.9 | 2.1*)
            # Cppcheck 2.8 removed the dependency on z3
            # Cppcheck 2.8 added optional support for utilizing Boost
            make_args="$make_args CPPFLAGS=-DHAVE_BOOST"
            ;;
        2.*)
            make_args="$make_args USE_Z3=yes"
            # Check that the Z3 development files appear to be installed
            # before trying to create z3_version.h. Otherwise we may
            # mislead the user as to what needs to be done.
            if [ ! -f /usr/include/z3.h ]; then
                echo "** libz3-dev (or equivalent) does not appear to be installed" >&2
            fi
            if [ ! -f /usr/include/z3_version.h ]; then
                create_z3_version_h
            fi
            ;;
    esac

    # Use all available CPUs
    if [ -f /proc/cpuinfo ]; then
        cpus=`grep ^processor /proc/cpuinfo | wc -l`
        if [ -n "$cpus" ]; then
            make_args="$make_args -j $cpus"
        fi
    fi

    echo "Making cppcheck..."
    # CFGDIR is needed for cppcheck before 1.86
    call_make $make_args

    echo "Installing cppcheck..."
    mkdir -p "$FILESDIR"
    call_make install $make_args
)
status=$?

if [ $status -eq 0 ]; then
    rm -rf "$workdir"
else
    "** Script failed. Work dir is $workdir" >&2
fi

exit $status
