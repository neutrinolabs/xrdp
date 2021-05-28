#!/bin/sh
set -eufx

FEATURE_SET=min
ARCH=amd64

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
    "

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
                    $PACKAGES_AMD64_MIN
                    libfuse-dev \
                    libjpeg-dev \
                    libmp3lame-dev \
                    libfdk-aac-dev \
                    libopus-dev \
                    libpixman-1-dev"
                ;;
            *)
                echo "unsupported feature set: $FEATURE_SET"
                exit 1;
                ;;
        esac
        ;;
    i386)
        PACKAGES="$PACKAGES \
            g++-multilib \
            gcc-multilib \
            libgl1-mesa-dev:i386 \
            libglu1-mesa-dev:i386 \
            libjpeg-dev:i386 \
            libmp3lame-dev:i386 \
            libfdk-aac-dev:i386 \
            libopus-dev:i386 \
            libpam0g-dev:i386 \
            libssl-dev:i386 \
            libx11-dev:i386 \
            libxext-dev:i386 \
            libxfixes-dev:i386 \
            libxrandr-dev:i386 \
            libxrender-dev:i386 \
            libfuse-dev:i386"
        
        dpkg --add-architecture i386
        dpkg --print-architecture
        dpkg --print-foreign-architectures
        apt-get update
        ;;
    *)
        echo "unsupported architecture: $ARCH"
        exit 1;
        ;;
esac

apt-get update
apt-get -yq \
    --no-install-suggests \
    --no-install-recommends \
    $APT_EXTRA_ARGS \
    install $PACKAGES
