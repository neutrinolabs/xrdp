# Test bed for the wlup prototype: builds xrdp and runs it against a
# headless sway (wlroots) compositor, all inside one container.
FROM debian:testing

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential autoconf automake libtool pkg-config nasm git ca-certificates \
        libssl-dev libpam0g-dev libx11-dev libxfixes-dev libxrandr-dev libxkbfile-dev \
        libjpeg-dev libimlib2-dev libfreetype-dev libopus-dev \
        libwayland-dev wayland-protocols libxkbcommon-dev \
        sway foot grim fonts-dejavu-core xkb-data openssl procps \
        freerdp3-x11 xvfb xdotool imagemagick wev jq \
    && rm -rf /var/lib/apt/lists/*

RUN useradd -m -u 1000 tester
