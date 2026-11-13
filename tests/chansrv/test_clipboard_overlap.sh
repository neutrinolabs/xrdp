#!/bin/sh
set -eu
if ! command -v xvfb-run || ! command -v Xvfb; then
    echo "SKIP: Xvfb and xvfb-run are required"
    exit 77
fi
exec xvfb-run -a sh -c '
    export XRDP_CLIPBOARD_TEST_DISPLAY="$DISPLAY"
    for mode in overlap mixed failure malformed clear deinit send-failure incr \
                idle idle-refuse incr-deinit destroy-incr pending-ack stale-timer \
                reconnect-timer clock-wrap slow-windows; do
        "$1" "$mode" || exit 1
    done
' sh ./test_clipboard_overlap
