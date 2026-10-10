# The wlup test bed with systemd as PID 1, so that sesman's PAM login goes
# through pam_systemd and gets a real logind session and /run/user/<uid>.
# Run with:
#   docker run -d --privileged --cgroupns=private --tmpfs /run \
#       --tmpfs /run/lock xrdp-wlup-systemd
FROM xrdp-wlup-dev

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        systemd systemd-sysv libpam-systemd dbus dbus-user-session \
        pipewire wireplumber pipewire-bin pipewire-module-xrdp \
        pulseaudio pulseaudio-utils \
    && rm -rf /var/lib/apt/lists/*

# A PulseAudio daemon for the RDP client side only (the microphone test):
# don't start it in users' sessions, they use PipeWire
RUN systemctl --global disable pulseaudio.socket pulseaudio.service

CMD ["/usr/lib/systemd/systemd"]
