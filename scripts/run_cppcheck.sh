#!/bin/sh

# Script to run cppcheck
#
# Usage: /path/to/run_cppcheck.sh [ -v CPPCHECK_VER] [<extra_opts_and_dirs>]
#
# - If <extra_opts_and_dirs> is missing, '.' is assumed
# - If -v CPPCHECK_VER is specified, that version of cppcheck is run from
#   ~/cppcheck.local (whether or not it's there!). Use install_cppcheck.sh
#   to install a new version.
#
# Environment (all optional):-
#
# CPPCHECK       : Override the default cppcheck command ('cppcheck').
#                  Ignored if -v is specified
# CPPCHECK_FLAGS : Override the default cppcheck flags

INSTALL_ROOT=~/cppcheck.local

# Figure out CPPCHECK setting, if any. Currently '-v' must be the first
# argument on the command line.
case "$1" in
    -v) # Version is separate parameter
        if [ $# -ge 2 ]; then
            CPPCHECK="$INSTALL_ROOT/$2/bin/cppcheck"
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
        CPPCHECK="$INSTALL_ROOT/${1#-v}/bin/cppcheck"
        shift 1
esac
if [ -z "$CPPCHECK" ]; then
    CPPCHECK=cppcheck
fi

# Supply default flags passed to cppcheck if necessary
if [ -z "$CPPCHECK_FLAGS" ]; then
    CPPCHECK_FLAGS="--quiet --force --std=c11 --std=c++11 --inline-suppr \
                    --enable=warning --error-exitcode=1 -i third_party \
                    --suppress=uninitMemberVar:ulalaca/ulalaca.cpp \
                    --suppress=shiftTooManyBits:libxrdp/xrdp_mppc_enc.c \
                    -I . -I common"
fi
CPPCHECK_FLAGS="$CPPCHECK_FLAGS -D__cppcheck__"

# Any options/directories specified?
if [ $# -eq 0 ]; then
    if [ -f /proc/cpuinfo ]; then
        cpus=$(grep '^processor' /proc/cpuinfo | wc -l)
    else
        cpus=2
    fi
    set -- -j $cpus .
fi

# Display the cppcheck version and command for debugging
"$CPPCHECK" --version && {
    echo "Command: $CPPCHECK $CPPCHECK_FLAGS" "$@"
    "$CPPCHECK" $CPPCHECK_FLAGS "$@"
}
