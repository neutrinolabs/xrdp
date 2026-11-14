# Test bed for xrdp's own Xorg sessions (xorgxrdp), on the wlup test image:
# run-backpressure.sh with BP_SESSION=xorg builds xorgxrdp against the
# xrdp under test, so the two always speak the same protocol, and runs an
# Xorg session with xterms instead of sway.
FROM xrdp-wlup-dev

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        xserver-xorg-core xserver-xorg-dev xterm xauth x11-utils \
        x11-xserver-utils dbus-x11 \
    && rm -rf /var/lib/apt/lists/*

# xorgxrdp devel at a fixed commit
ARG XORGXRDP_COMMIT=59d0735c4321b5baff3feaa980a01f78a1c1e96f
ADD https://github.com/neutrinolabs/xorgxrdp/archive/${XORGXRDP_COMMIT}.tar.gz /usr/src/xorgxrdp.tar.gz
RUN mkdir /usr/src/xorgxrdp && \
    tar -xzf /usr/src/xorgxrdp.tar.gz -C /usr/src/xorgxrdp --strip-components=1 && \
    echo ${XORGXRDP_COMMIT} > /usr/src/xorgxrdp/SOURCE_COMMIT
