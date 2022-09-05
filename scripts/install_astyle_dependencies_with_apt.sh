#!/bin/sh
set -eufx

PACKAGES="subversion cmake"

apt-get update
apt-get -yq --no-install-suggests --no-install-recommends install $PACKAGES
