Clipboard overlap regression
============================

Run after configure and building common/libcommon:

    make -C tests/chansrv check

Requires Xvfb and xvfb-run; skipped when these tools are unavailable.
The wrapper supplies a disposable display. Direct execution without the
wrapper's display marker is refused, to avoid replacing a live clipboard.

The test includes the production clipboard.c handlers and uses two real
X11 requestor connections. Only the Windows channel transport and unused
FUSE/file helpers are mocked. Baseline overlapping requests lose the first
notification and deliver two to the second requestor.

Checks: distinct text payloads, text/BMP routing, failed and malformed replies,
clipboard ownership loss, deinit refusals without an owner-side sync, send
failure, and INCR chunks through the zero-length terminator. This is not a
full RDP/client or file-list conversion test.

Timer cases use a recorded native-timer callback and simulated monotonic clock:
abandoned readers, progress over longer than five seconds in total, pending
acknowledgements, destruction/deinit, stale callbacks from an earlier transfer,
and a sixty-second Windows reply delay without an INCR timer. Idle-refuse omits
owner-side synchronization to verify callback XFlush delivers queued refusals.

Optional stress run (same disposable-display boundary):

    xvfb-run -a sh -c 'export XRDP_CLIPBOARD_TEST_DISPLAY="$DISPLAY"; ./test_clipboard_overlap stress'

Uses 64 real X11 requestors, queues each burst before any Windows reply, and
repeats 100 times. Alternates text/BMP and injects failed replies; checks FIFO
order, notification counts/requestors/properties, text content, BMP type/length/
signature, empty queue and no duplicate events. Later text values repeat; this
does not prove unique same-format payload association or exercise full RDP.
