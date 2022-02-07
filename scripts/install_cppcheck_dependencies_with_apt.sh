#!/bin/sh
set -eufx

PACKAGES="libz3-dev z3"
        
apt-get update
apt-get -yq --no-install-suggests --no-install-recommends install $PACKAGES
