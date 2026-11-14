# Builds a FreeRDP X11 client from source, to test client fixes against
# xrdp + wlup
FROM xrdp-wlup-dev

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        cmake ninja-build libx11-dev libxext-dev libxcursor-dev libxinerama-dev \
        libxrandr-dev libxkbfile-dev libxi-dev libxrender-dev libxfixes-dev \
        libxdamage-dev libxtst-dev libxv-dev libkrb5-dev libcjson-dev zlib1g-dev \
        liburiparser-dev \
    && rm -rf /var/lib/apt/lists/*
