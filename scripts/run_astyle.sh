#!/bin/sh

# Script to run astyle on the code
#
# Usage: /path/to/run_astyle.sh
#
# Note: the script must be run from the root directory of the xrdp repository

INSTALL_ROOT=~/astyle.local
ASTYLE_FROM_XRDP=$INSTALL_ROOT/3.4.12/usr/bin/astyle
MIN_ASTYLE_VER="3.1"

# ----------------------------------------------------------------------------
# U S A G E
# ----------------------------------------------------------------------------
usage()
{
    echo "** Usage: $0"
    echo "   e.g. $0"
} >&2

# ----------------------------------------------------------------------------
# M A I N
# ----------------------------------------------------------------------------
if [ $# -ne 0 ]; then
    usage
    exit 1
fi

# check if the built-in astyle meets the minimum requrements
ASTYLE_FROM_OS_VER_OUTPUT=`astyle --version | grep "Artistic Style Version" | cut -d' ' -f4`

ASTYLE=""
ERROR_MESSAGE=""
if [ ! -z "$ASTYLE_FROM_OS_VER_OUTPUT" ]; then
    # astyle is installed, so check if it's version meets the minimum requirements
    LOWEST_VERSION=`echo -e "$MIN_ASTYLE_VER\n$ASTYLE_FROM_OS_VER_OUTPUT" | sort -V | head -n1`
    if [ "$MIN_ASTYLE_VER" = "$LOWEST_VERSION" ]; then
        ASTYLE=astyle
    else
        ERROR_MESSAGE="The version of astyle installed does not meet the minimum version requirement: >= $MIN_ASTYLE_VER "
    fi
else
    ERROR_MESSAGE="astyle is not installed on the system path"
fi

if [ -z "$ASTYLE" ]; then
    # astyle from the os is invlid, fallback to the xrdp version if it is installed
    if [ -x "$ASTYLE_FROM_XRDP" ]; then
        ASTYLE="$ASTYLE_FROM_XRDP"
        ERROR_MESSAGE=""
    else
        ERROR_MESSAGE="${ERROR_MESSAGE}\nastyle $MIN_ASTYLE_VER is not installed at the expected path: $ASTYLE_FROM_XRDP"
    fi
fi

if [ ! -z "$ERROR_MESSAGE" ]; then
    echo "$ERROR_MESSAGE"
    exit 1
fi

if [ ! -f "astyle_config.as" ]; then
    echo "$0 must be run from the root xrdp repository directory which "
    echo "contains the 'astyle_config.as' file."
    exit 2
fi

ASTYLE_FLAGS="--options=astyle_config.as --exclude=third_party ./\*.c ./\*.h"

# Display the astyle version and command for debugging
"$ASTYLE" --version && {
    echo "Command: $ASTYLE $ASTYLE_FLAGS"
    "$ASTYLE" $ASTYLE_FLAGS
}
