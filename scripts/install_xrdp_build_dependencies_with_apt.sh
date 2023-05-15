#!/bin/sh
set -eufx

FEATURE_SET=min
ARCH=amd64

# See apt.conf(5)
DEBUG_OPTS=
#DEBUG_OPTS="$DEBUG_OPTS -oDebug::pkgProblemResolver=1"

# Given a list of i386 packages, this call makes sure the amd64 variants
# of the development libraries are not installed
remove_64bit_libdev_packages()
{
    flags=$-
    set +x  ;# Disable tracing for this func
    rlist=""
    for p in $PACKAGES; do
        # Identify packages starting with 'lib' and ending with '-dev:i386'
        if [ "${p#lib}" != "$p" -a "${p%-dev:i386}" != "$p" ]; then
            p64=${p%i386}amd64
            if dpkg -s $p64 >/dev/null 2>&1 ; then
                rlist="$rlist ${p%i386}amd64"
            fi
        fi
    done
    if [ -n "$rlist" ]; then
        echo "** Some 64-bit development packages appear to clash with i386 packages"
        echo "**$rlist"
        apt-get remove $rlist || :
    fi
    # Restore tracing, if enabled on entry
    set -$flags
}

# ----------------------------------------------------------------------------
# M A I N
# ----------------------------------------------------------------------------

if [ $# -ge 1 ]; then
    FEATURE_SET="$1"
    shift
fi
if [ $# -ge 1 ]; then
    ARCH="$1"
    shift
fi
APT_EXTRA_ARGS="$@"

# common build tools for all architectures and feature sets
PACKAGES=" \
    autoconf \
    automake \
    clang \
    gcc \
    g++ \
    libtool \
    make \
    nasm \
    pkg-config \
    check \
    libcmocka-dev
    "

# libfreetype-dev package was renamed from libfreetype6-dev in older
# versions
case `lsb_release -si`/`lsb_release -sr` in
    Debian/10) LIBFREETYPE_DEV=libfreetype6-dev ;;
    Ubuntu/18.*) LIBFREETYPE_DEV=libfreetype6-dev ;;
    *) LIBFREETYPE_DEV=libfreetype-dev
esac

case "$ARCH"
in
    amd64)
        PACKAGES_AMD64_MIN=" \
            libpam0g-dev \
            libssl-dev \
            libx11-dev \
            libxrandr-dev \
            libxfixes-dev"

        case "$FEATURE_SET"
        in
            min)
                PACKAGES="$PACKAGES $PACKAGES_AMD64_MIN"
                ;;
            max)
                PACKAGES="$PACKAGES \
                    $PACKAGES_AMD64_MIN \
                    $LIBFREETYPE_DEV \
                    libfuse-dev \
                    libjpeg-dev \
                    libmp3lame-dev \
                    libfdk-aac-dev \
                    libimlib2-dev \
                    libopus-dev \
                    libpixman-1-dev"
                ;;
            *)
                echo "unsupported feature set: $FEATURE_SET"
                exit 1;
                ;;
        esac
        apt-get update
        apt-get upgrade
        ;;

    i386)
        # This list is not as complete as the amd64 list. It currently
        # supports 32-bit CI building only, rather than being a generic
        # build support tool.
        # - Ubuntu 18.04 -> 20.04
        #       Removed fdk-aac-dev:i386 and libfuse-dev:i386
        PACKAGES="$PACKAGES \
            g++-multilib \
            gcc-multilib \
            $LIBFREETYPE_DEV:i386 \
            libgl1-mesa-dev:i386 \
            libglu1-mesa-dev:i386 \
            libjpeg-dev:i386 \
            libimlib2-dev:i386 \
            libmp3lame-dev:i386 \
            libopus-dev:i386 \
            libpam0g-dev:i386 \
            libssl-dev:i386 \
            libx11-dev:i386 \
            libxext-dev:i386 \
            libxfixes-dev:i386 \
            libxrandr-dev:i386 \
            libxrender-dev:i386 \
            libsubunit-dev:i386 \
            check:i386 \
            libcmocka-dev:i386"

        dpkg --add-architecture i386
        dpkg --print-architecture
        dpkg --print-foreign-architectures
        apt-get update
        remove_64bit_libdev_packages $PACKAGES
        apt-get install libc6:i386 libgcc-s1:i386 libstdc++6:i386 libatomic1:i386
        ;;
    *)
        echo "unsupported architecture: $ARCH"
        exit 1;
        ;;
esac

apt-get -yq \
    --no-install-suggests \
    --no-install-recommends \
    $DEBUG_OPTS \
    $APT_EXTRA_ARGS \
    install $PACKAGES
