#!/bin/sh
set -eufx

PACKAGES="libz3-dev z3"

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

case "$CPPCHECK_VER" in
        1.*)
            # no dependencies
            ;;
        2.8 | 2.9 | 2.1*)
            # Cppcheck 2.8 removed the dependency on z3
            ;;
        2.*)
            apt-get update
            apt-get -yq --no-install-suggests --no-install-recommends install $PACKAGES
            ;;
esac
