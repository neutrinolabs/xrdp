#!/bin/bash
# Builds the test images and runs every wlup end-to-end suite, in
# parallel, each in its own container:
#
#   sway      plain container, with /dev/fuse for the drive test
#   systemd   systemd as PID 1: real PAM -> logind session, microphone
#   gnome     systemd as PID 1, headless GNOME through the Mutter backend
#
# Usage: wlup/test/run-all.sh [-n] [output dir]
#   -n  do not rebuild the images
#
# Exit status: 0 if every suite ran and passed, 1 otherwise. Logs and
# screenshots of each suite are kept in <output dir>/<suite>/.
set -u

TEST_DIR=$(cd "$(dirname "$0")" && pwd)
SRC=$(cd "$TEST_DIR/../.." && pwd)

BUILD=1
if [ "${1:-}" = "-n" ]; then
    BUILD=0
    shift
fi
OUT=${1:-$(mktemp -d /tmp/wlup-test.XXXXXX)}
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)
TAG=wlup-test-$$

# --- images (each one is built on the previous) -------------------------------
if [ $BUILD = 1 ]; then
    for img in "xrdp-wlup-dev $TEST_DIR" \
               "xrdp-wlup-systemd $TEST_DIR/systemd" \
               "xrdp-wlup-gnome $TEST_DIR/gnome" \
               "xrdp-wlup-mutter $TEST_DIR/mutter"; do
        set -- $img
        echo "building $1"
        if ! docker build -q -t "$1" "$2" > "$OUT/build-$1.log" 2>&1; then
            echo "FAILED to build $1, see $OUT/build-$1.log"
            exit 1
        fi
    done
fi

cleanup() {
    docker rm -f "$TAG-systemd" "$TAG-gnome" > /dev/null 2>&1
}
trap cleanup EXIT

# Runs a suite in a container whose PID 1 is systemd
# run_systemd <suite> <image> <script inside /src>
run_systemd() {
    docker run -d --name "$TAG-$1" --privileged --cgroupns=private \
        --tmpfs /run --tmpfs /run/lock \
        -v "$SRC":/src:ro -v "$OUT/$1":/out "$2" /usr/lib/systemd/systemd \
        > /dev/null &&
    sleep 5 &&
    docker exec "$TAG-$1" bash -c \
        "bash $3 /src /out > /out/run.log 2>&1; chmod -R a+r /out"
    docker rm -f "$TAG-$1" > /dev/null 2>&1
}

mkdir -p "$OUT/sway" "$OUT/systemd" "$OUT/gnome"
echo "running the suites in parallel, output in $OUT"

docker run --rm --device /dev/fuse --cap-add SYS_ADMIN \
    --security-opt apparmor:unconfined \
    -v "$SRC":/src:ro -v "$OUT/sway":/out xrdp-wlup-dev \
    bash -c 'bash /src/wlup/test/run-e2e.sh /src /out > /out/run.log 2>&1;
             chmod -R a+r /out' &
run_systemd systemd xrdp-wlup-systemd /src/wlup/test/run-e2e.sh &
run_systemd gnome xrdp-wlup-mutter /src/wlup/test/mutter/run-e2e-mutter.sh &
wait

# --- summary --------------------------------------------------------------
status=0
for suite in sway systemd gnome; do
    log="$OUT/$suite/run.log"
    result=$(grep -E '^passed [0-9]+, failed [0-9]+' "$log" 2>/dev/null | tail -1)
    printf '%-8s %s\n' "$suite" "${result:-did not finish, see $log}"
    grep -E '^FAIL' "$log" 2>/dev/null | sed 's/^/         /'
    case "$result" in
        *"failed 0") ;;
        *) status=1 ;;
    esac
done
exit $status
