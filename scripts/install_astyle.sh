#!/bin/sh

# Script to install a version of astyle in ~/astyle.local/
#
# Used by CI builds
#
# Currently only supports git repos as sources
#
# Usage: /path/to/install_astyle.sh <astyle-git-repo> <version-tag>

INSTALL_ROOT=~/astyle.local

# ----------------------------------------------------------------------------
# U S A G E
# ----------------------------------------------------------------------------
usage()
{
    echo "** Usage: $0 <svn-tags-url> <tag-name>"
    echo "   e.g. $0 https://svn.code.sf.net/p/astyle/code/tags 3.1"
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
    log=`mktemp /tmp/astyle-log.XXXXXXXXXX`
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
ASTYLE_VER="$2"

# Already installed?
exe=$INSTALL_ROOT/$ASTYLE_VER/usr/bin/astyle
if [ -x "$exe" ]; then
    echo "astyle version $ASTYLE_VER is already installed at $exe" >&2
    exit 0
fi

workdir=`mktemp -d /tmp/astyle.XXXXXXXXXX`
if [ -z "$workdir" ]; then
    echo "** Unable to create temporary working directory" 2>&1
    exit 1
fi

# Use a sub-process for the next bit to restrict the scope of 'set -e'
(
    set -e ; # Exit sub-process on first error

    # Put everything in this directory
    FILESDIR=$INSTALL_ROOT/$ASTYLE_VER

    svn checkout ${REPO_URL}/${ASTYLE_VER}/AStyle $workdir

    cd $workdir

    make_args="DESTDIR=$FILESDIR"
    
    echo "Creating Makefiles..."
    cmake .
    
    echo "Making astyle..."
    call_make $make_args

    echo "Installing astyle..."
    mkdir -p $FILESDIR
    call_make install $make_args
    # make install DESTDIR=~/astyle.local/3.1
)
status=$?

if [ $status -eq 0 ]; then
    rm -rf $workdir
else
    "** Script failed. Work dir is $workdir" >&2
fi

exit $status
