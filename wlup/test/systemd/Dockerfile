# The wlup test bed with systemd as PID 1, so that sesman's PAM login goes
# through pam_systemd and gets a real logind session and /run/user/<uid>.
# Run with:
#   docker run -d --privileged --cgroupns=private --tmpfs /run \
#       --tmpfs /run/lock xrdp-wlup-systemd
FROM xrdp-wlup-dev

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        systemd systemd-sysv libpam-systemd dbus dbus-user-session \
    && rm -rf /var/lib/apt/lists/*

CMD ["/usr/lib/systemd/systemd"]
