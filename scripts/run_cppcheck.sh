#!/bin/sh

# Script to run cppcheck
#
# Usage: /path/to/run_cppcheck.sh [-f] [ -v CPPCHECK_VER] [<extra_opts_and_dirs>]
#
# - If <extra_opts_and_dirs> is missing, '.' is assumed
# - If '-f' is specified, the exhaustive check level is used. This can take
#   an hour or two.
# - If -v CPPCHECK_VER is specified, that version of cppcheck is run from
#   ~/cppcheck.local (whether or not it's there!). Use install_cppcheck.sh
#   to install a new version.
#
# Environment (all optional):-
#
# CPPCHECK       : Override the default cppcheck command ('cppcheck').
#                  Ignored if -v is specified
# CPPCHECK_FLAGS : Override the default cppcheck flags
# CPPCHECK_QUICK : If non-zero, disables the exhaustive check level.
#                  Useful when developing

# ----------------------------------------------------------------------------
# C P P C H E C K   V E R S I O N   S T R
#
# Get the cppcheck version, and then convert to AABBCC for comparisons
# where AA is the major version, BB is the minor version and CC is the
# revision
# For example, cppcheck version 2.13.0 produces 021300
# ----------------------------------------------------------------------------
CppcheckVerCompareStr()
{
    # shellcheck disable=SC2046
    set -- $($CPPCHECK --version 2>/dev/null)
    while [ $# -gt 0 ]; do
        case "$1" in
            [0123456789]*) break;;
            *) shift
        esac
    done

    if [ $# -eq 0 ]; then
        echo 000000 ; # Something has gone wrong
    else
        case "$1" in
            *.*.*) echo "$1" | awk -F.  '{printf "%02d%02d%02d",$1,$2,$3 }' ;;
            *.*)   echo "$1" | awk -F.  '{printf "%02d%02d00",$1,$2 }' ;;
            *)     printf '%02d0000\n' "$1"
        esac
    fi
}

INSTALL_ROOT=~/cppcheck.local

# Exhaustive test?
if [ "$1" = "-f" ]; then
    check_level=exhaustive
    shift
else
    check_level=normal
fi

# Figure out CPPCHECK setting, if any.
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
                    --suppress=shiftTooManyBits:libxrdp/xrdp_mppc_enc.c"

    # Check for flags added in later versions
    CPPCHECK_COMPARE_VERSION=$(CppcheckVerCompareStr)
    if [ "$CPPCHECK_COMPARE_VERSION" -ge 021100 ]; then
        if [ "$check_level" = "normal" ]; then
            # Disable warnings related to the check level
            CPPCHECK_FLAGS="$CPPCHECK_FLAGS \
                --suppress=normalCheckLevelMaxBranches"
        fi
        CPPCHECK_FLAGS="$CPPCHECK_FLAGS --check-level=$check_level"
    else
        # This is echoed later
        check_level="Default (option not supported)"
    fi

    CPPCHECK_FLAGS="$CPPCHECK_FLAGS -I . -I common"
fi
CPPCHECK_FLAGS="$CPPCHECK_FLAGS -D__cppcheck__"

# Any options/directories specified?
if [ $# -eq 0 ]; then
    if [ -f /proc/cpuinfo ]; then
        cpus=$(grep -c '^processor' /proc/cpuinfo)
    else
        cpus=2
    fi
    set -- -j $cpus .
fi

# Display the cppcheck version and command for debugging
"$CPPCHECK" --version && {
    echo "Command: $CPPCHECK $CPPCHECK_FLAGS" "$@"
    echo "Check level: $check_level"

    # shellcheck disable=SC2086
    "$CPPCHECK" $CPPCHECK_FLAGS "$@"
}
