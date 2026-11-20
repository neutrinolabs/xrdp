# Drive redirection and microphone tests shared by run-e2e.sh (sway) and
# run-e2e-mutter.sh (GNOME), sourced by them. Both features go through
# chansrv, which sesman starts for these sessions too.
#
# Drive redirection: the RDP client shares $DRIVE_DIR as 'share'. chansrv
# mounts it with FUSE in the session (sesman.ini FuseMountName, by default
# ~/thinclient_drives). From a terminal in the session, a command reads a
# file the client put there, and writes one back. xrdp must be built with
# --enable-fuse, and the container needs /dev/fuse: the systemd and GNOME
# containers are privileged; the plain one must be run with
#   --device /dev/fuse --cap-add SYS_ADMIN --security-opt apparmor:unconfined
# Without /dev/fuse the drive checks are skipped.
#
# Microphone: the RDP client records from a PulseAudio daemon of its own,
# whose default source is the monitor of a null sink playing a 440 Hz
# tone. The null sink's clock paces the tone, so the client gets it in
# real time. In the session, the recording comes out of the xrdp-source
# node of the user's PipeWire (pipewire-module-xrdp); it is recorded with
# pw-cat and checked with sox: not silent, and about 440 Hz. This needs a
# PipeWire session (systemd as PID 1); otherwise the microphone checks are
# skipped.
#
# Call drive_mic_setup before connecting, and add $DRIVE_MIC_OPTS to the
# client's command line (and PULSE_SERVER to its environment, which
# drive_mic_setup exports). drive_test needs a terminal in the session
# with the keyboard focus. Results go to $OUT/drive-* and $OUT/mic-*;
# drive_mic_checks() checks them.

DRIVE_DIR=/tmp/rdp-share
MIC_TONE_HZ=440
CLIENT_PULSE=/tmp/client-pulse

drive_mic_setup() {
    DRIVE_MIC_OPTS=
    HAVE_FUSE=
    HAVE_MIC=

    if [ -c /dev/fuse ]; then
        HAVE_FUSE=1
        mkdir -p "$DRIVE_DIR"
        # 日本 in UTF-8, as octal escapes for printf
        printf 'from-client-42\n\346\227\245\346\234\254\n' \
            > "$DRIVE_DIR/from-client.txt"
        DRIVE_MIC_OPTS="$DRIVE_MIC_OPTS /drive:share,$DRIVE_DIR"
    fi

    if [ "$(cat /proc/1/comm)" = systemd ] && command -v pulseaudio > /dev/null &&
            command -v pw-cat > /dev/null; then
        HAVE_MIC=1
        mkdir -p "$CLIENT_PULSE"
        sox -n -r 44100 -c 2 -b 16 "$CLIENT_PULSE/tone.wav" \
            synth 120 sine $MIC_TONE_HZ vol 0.5
        # Runs as root for the client only: no system mode, no session
        HOME=$CLIENT_PULSE XDG_RUNTIME_DIR=$CLIENT_PULSE \
            pulseaudio --daemonize=no --system=false -n --use-pid-file=no \
            --exit-idle-time=-1 --disallow-exit \
            -L "module-native-protocol-unix socket=$CLIENT_PULSE/native auth-anonymous=1" \
            -L "module-null-sink sink_name=mic" \
            > "$OUT/mic-client-pulse.log" 2>&1 &
        export PULSE_SERVER=unix:$CLIENT_PULSE/native
        for _ in $(seq 20); do
            pactl info > /dev/null 2>&1 && break
            sleep 0.5
        done
        pactl set-default-source mic.monitor
        paplay -d mic "$CLIENT_PULSE/tone.wav" > "$OUT/mic-paplay.log" 2>&1 &
        DRIVE_MIC_OPTS="$DRIVE_MIC_OPTS /microphone:sys:pulse"
    fi
    echo "drive test: ${HAVE_FUSE:-skipped (no /dev/fuse)}," \
        "microphone test: ${HAVE_MIC:-skipped (needs systemd, PipeWire and pulseaudio)}"
}

# Reads the client's file and writes one back, from the session's terminal
drive_test() {
    [ -n "$HAVE_FUSE" ] || return 0
    local mnt=/home/tester/thinclient_drives/share
    for _ in $(seq 20); do
        su tester -c "test -r $mnt/from-client.txt" 2>/dev/null && break
        sleep 1
    done
    su tester -c "ls -la $mnt" > "$OUT/drive-ls.txt" 2>&1 || true
    grep -a " $mnt \| ${mnt%/share} " /proc/mounts > "$OUT/drive-mounts.txt" || true
    xdotool type --delay 40 \
        "cp ~/thinclient_drives/share/from-client.txt /tmp/drive-read.txt; printf 'from-session-42\\n' > ~/thinclient_drives/share/from-session.txt"
    xdotool key Return
    sleep 3
    printf 'from-session-42\n' > "$OUT/drive-s2c-expected.txt"
    cp /tmp/drive-read.txt "$OUT/drive-c2s.txt" 2>/dev/null || true
    cp "$DRIVE_DIR/from-session.txt" "$OUT/drive-s2c.txt" 2>/dev/null || true
}

# Records the microphone in the session, as the user
mic_test() {
    [ -n "$HAVE_MIC" ] || return 0
    local run
    run="export XDG_RUNTIME_DIR=/run/user/$(id -u tester)"
    for _ in $(seq 20); do
        su tester -c "$run; pw-cli ls Node" 2>/dev/null |
            grep -q 'node.name = "xrdp-source"' && break
        sleep 1
    done
    su tester -c "$run; timeout 6 pw-cat --record --raw --target xrdp-source \
        --format s16 --rate 44100 --channels 2 -" > "$OUT/mic.raw" \
        2> "$OUT/mic-pw-cat.log" || true
    # Skip the first 2 seconds: chansrv opens the client's microphone
    # when the recording starts
    sox -t raw -r 44100 -c 2 -b 16 -e signed "$OUT/mic.raw" -n \
        trim 2 remix 1 stat > "$OUT/mic-stat.txt" 2>&1 || true
    MIC_RMS=$(awk '/RMS +amplitude/ { print $3 }' "$OUT/mic-stat.txt")
    MIC_FREQ=$(awk '/Rough +frequency/ { print $3 }' "$OUT/mic-stat.txt")
    echo "microphone: $(stat -c %s "$OUT/mic.raw") bytes, RMS ${MIC_RMS:-?}," \
        "rough frequency ${MIC_FREQ:-?} Hz"
}

drive_mic_checks() {
    if [ -n "$HAVE_FUSE" ]; then
        check "drive: client file read in the session (UTF-8)" \
            'cmp -s "$OUT/drive-c2s.txt" "$DRIVE_DIR/from-client.txt"'
        check "drive: session file written to the client" \
            'cmp -s "$OUT/drive-s2c.txt" "$OUT/drive-s2c-expected.txt"'
    fi
    if [ -n "$HAVE_MIC" ]; then
        check "microphone: the session records sound (RMS > 0.05)" \
            'awk -v r="$MIC_RMS" "BEGIN { exit !(r + 0 > 0.05) }"'
        check "microphone: the recording is the client's ${MIC_TONE_HZ} Hz tone" \
            'awk -v f="$MIC_FREQ" "BEGIN { exit !(f >= 400 && f <= 480) }"'
    fi
}
