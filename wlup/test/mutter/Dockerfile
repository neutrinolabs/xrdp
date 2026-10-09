# Test bed for the Mutter backend of wlup: headless GNOME Shell plus the
# libraries the backend builds against (sd-bus, PipeWire, libei)
FROM xrdp-wlup-gnome

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        libsystemd-dev libpipewire-0.3-dev libei-dev \
    && rm -rf /var/lib/apt/lists/*
