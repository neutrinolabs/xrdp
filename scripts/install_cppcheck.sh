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
        make "$@" >$log 2>&1
        status=$?
        if [ $status -ne 0 ]; then
            cat $log >&2
        fi
        rm $log
    fi

    # Re-enable `set -e` if active before
    $set_entry_opts

    return $status
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

    # CFGDIR is needed for cppcheck before 1.86
    make_args="FILESDIR=$FILESDIR PREFIX=$FILESDIR CFGDIR=$FILESDIR"

    # See https://stackoverflow.com/questions/
    #     791959/download-a-specific-tag-with-git
    git clone -b $CPPCHECK_VER --depth 1 $REPO_URL $workdir

    cd $workdir
    echo "Making cppcheck..."
    # CFGDIR is needed for cppcheck before 1.86
    call_make $make_args

    echo "Installing cppcheck..."
    mkdir -p $FILESDIR
    call_make install $make_args
)
status=$?

if [ $status -eq 0 ]; then
    rm -rf $workdir
else
    "** Script failed. Work dir is $workdir" >&2
fi

exit $status
