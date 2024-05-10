#!/bin/sh

curl -s -H 'Cache-Control: no-cache' \
    https://raw.githubusercontent.com/wiki/neutrinolabs/xrdp/NEWS-v0.10.md > \
    $(git rev-parse --show-superproject-working-tree --show-toplevel | head -1)/NEWS.md
