#!/bin/sh

# Script to run astyle on the code
#
# Usage: /path/to/run_astyle.sh [ -v ASTYLE_VER]
#
# - If -v ASTYLE_VER is specified, that version of astyle is run from
#   ~/astyle.local (whether or not it's there!). Use install_astyle.sh
#   to install a new version.

# Note: the script must be run from the root directory of the xrdp repository

INSTALL_ROOT=~/astyle.local
MIN_ASTYLE_VER="3.1"

# ----------------------------------------------------------------------------
# U S A G E
# ----------------------------------------------------------------------------
usage()
{
    echo "** Usage: $0 [ -v version]"
    echo "   e.g. $0 -v 3.4.12"
} >&2

# ----------------------------------------------------------------------------
# M A I N
# ----------------------------------------------------------------------------
# Figure out ASTYLE setting, if any. Currently '-v' must be the first
# argument on the command line.
case "$1" in
    -v) # Version is separate parameter
        if [ $# -ge 2 ]; then
            ASTYLE="$INSTALL_ROOT/$2/usr/bin/astyle"
            shift 2
        else
            echo "** ignoring '-v' with no arg" >&2
            shift 1
        fi
        ;;
    -v*) # Version is in same parameter
        # ${parameter#word} is not supported by classic Bourne shell,
        # but it is on bash, dash, etc. If it doesn't work on your shell,
        # don't use this form!
        ASTYLE="$INSTALL_ROOT/${1#-v}/usr/bin/astyle"
        shift 1
esac

if [ -z "$ASTYLE" ]; then
    ASTYLE=astyle
fi

if [ $# -ne 0 ]; then
    usage
    exit 1
fi


# check if the selected astyle meets the minimum requrements
ASTYLE_VER_OUTPUT=`$ASTYLE --version 2>/dev/null | grep "Artistic Style Version" | cut -d' ' -f4`

if [ ! -z "$ASTYLE_VER_OUTPUT" ]; then
    # Check the version meets the minimum requirements
    LOWEST_VERSION=`{ echo "$MIN_ASTYLE_VER" ; echo "$ASTYLE_VER_OUTPUT"; } | sort -V | head -n1`
    if [ "$MIN_ASTYLE_VER" != "$LOWEST_VERSION" ]; then
        ERROR_MESSAGE="The version of astyle installed does not meet the minimum version requirement: >= $MIN_ASTYLE_VER "
    fi
elif [ "$ASTYLE" = astyle ]; then
    ERROR_MESSAGE="astyle is not installed on the system path"
else
    ERROR_MESSAGE="Can't find $ASTYLE"
fi

if [ ! -z "$ERROR_MESSAGE" ]; then
    echo "$ERROR_MESSAGE" >&2
    exit 1
fi

if [ ! -f "astyle_config.as" ]; then
    echo "$0 must be run from the root xrdp repository directory which " >&2
    echo "contains the 'astyle_config.as' file." >&2
    exit 2
fi

ASTYLE_FLAGS="--options=astyle_config.as --exclude=third_party ./\*.c ./\*.h"

# Display the astyle version and command for debugging
"$ASTYLE" --version && {
    echo "Command: $ASTYLE $ASTYLE_FLAGS"
    "$ASTYLE" $ASTYLE_FLAGS
}

exit $?
