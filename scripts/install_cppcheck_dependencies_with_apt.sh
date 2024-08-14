#!/bin/sh
set -eufx

# these are the packages necessary to run ./configure so config_ac.h is generated
PACKAGES="libpam0g-dev libxfixes-dev libxrandr-dev libxkbfile-dev nasm"

usage()
{
    echo "** Usage: $0 <version-tag>"
    echo "   e.g. $0 1.90"
} >&2

if [ $# -ne 1 ]; then
    usage
    exit 1
fi
CPPCHECK_VER="$1"

apt-get update

case "$CPPCHECK_VER" in
        1.*)
            # no dependencies
            ;;
        2.8 | 2.9 | 2.1*)
            # Cppcheck 2.8 removed the dependency on z3
            # Cppcheck 2.8 added optional support for utilizing Boost
            PACKAGES="$PACKAGES libboost-container-dev"
            ;;
        2.*)
            PACKAGES="$PACKAGES libz3-dev z3"
            ;;
esac

apt-get -yq --no-install-suggests --no-install-recommends install $PACKAGES
