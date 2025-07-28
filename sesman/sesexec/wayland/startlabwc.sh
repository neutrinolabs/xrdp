#!/bin/sh

# Run this script from a command line
#
# A list of environment variables are written to a filename. The file
# is created atomically.
ok=true

# shellcheck disable=SC2043
for command in labwc; do
    if ! command -v $command ;then
        echo "*** Command '$command' is missing" >&2
        ok=false
    fi
done >/dev/null

# shellcheck disable=SC2043
for envvar in XRDP_SESSION XRDP_SOCKET_PATH; do
    eval val="\$$envvar"
    if [ -z "$val" ]; then
        echo "*** Environment variable '$envvar' is missing" >&2
        ok=false
    fi
done

# Get the directory this script is run from
this_dir=$(cd "$(dirname "$0")" && pwd)
if [ -z "$this_dir" ]; then
    echo "** Can't work out the directory for $0" >&2
    ok=false
fi

if ! "$ok" ; then
    exit 1
fi

# Get the name for the environment file and make sure it's gone
env_file="$XRDP_SOCKET_PATH/xrdp.$XRDP_SESSION.env"
rm -f "$env_file"

# Set up environment for labwc
export WLR_BACKENDS=headless ;# Run with no backing hardware
export LABWC_FALLBACK_OUTPUT=NOOP-fallback ; # Always have an output
export LABWC_UPDATE_ACTIVATION_ENV=1 ;# Update dbus-daemon session variables

# Start labwc and get the environment in 'env_file'
exec labwc --start "$this_dir/dumpenv.sh $env_file"
