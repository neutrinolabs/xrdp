#!/bin/sh
#
# Script to generate release tarball
#
# This script will generate release tarball. If the repository HEAD is not
# tagged, the script will be aborted. Target GitHub repository, branch can be
# customized by GH_* envirnment variable.
#
# Usage:
#   Just execute the script. This script is intended to be run by maintainers
#   so build dependencies are not taken care of.
#
#     $ ./scripts/make_release_tarball.sh
#     (snip)
#     ===============================================
#     Release tarball has been generated at:
#     /tmp/tmp.q8kme7m7/xrdp-0.9.22.1.tar.gz
#
#     CHECKSUM:
#     3ca220d6941ca6dab6a8bd1b50fcffffcf386ce01fbbc350099fdc83684380e0  xrdp-0.9.22.1.tar.gz
#
#     Copy tarball to /home/meta/github/metalefty/xrdp? [y/N] y
#
#   If the script is executed with BATCH=yes, the full path of the release
#   tarball will be printed on the last line. Clip it using tail command.
#
#     $ BATCH=yes ./scripts/make_release_tarball.sh | tail -n1

set -e

: ${GH_ACCOUNT:=neutrinolabs}
: ${GH_PROJECT:=xrdp}
: ${GH_BRANCH:=v0.9}
: ${BATCH:=""}

WRKDIR=$(mktemp -d)

# clean checkout
git clone --recursive --branch "${GH_BRANCH}" "https://github.com/${GH_ACCOUNT}/${GH_PROJECT}.git" "${WRKDIR}"

cd "${WRKDIR}"
git diff --no-ext-diff --quiet --exit-code          # check if there's no changes on working tree
RELVER=$(git describe --tags --exact-match HEAD)    # check if working HEAD refs a release tag
./bootstrap
./configure
make distcheck

echo
echo ===============================================
echo Release tarball has been generated at:
echo ${WRKDIR}/xrdp-${RELVER#v}.tar.gz
echo
echo CHECKSUM:
sha256sum xrdp-${RELVER#v}.tar.gz
echo

if [ -z "${BATCH}" ]; then
    echo -n "Copy tarball to ${OLDPWD}? [y/N] "
    read -t 60 copy_tarball
    case "${copy_tarball}" in
        [Yy]*) cp -i xrdp-${RELVER#v}.tar.gz "${OLDPWD}"; exit ;;
    esac
else
    echo ${WRKDIR}/xrdp-${RELVER#v}.tar.gz
fi
