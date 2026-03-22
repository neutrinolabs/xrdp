#!/bin/bash
# Start xrdp server for testing
# This script starts the xrdp server daemon directly for testing purposes
# In normal use, click the menu bar icon to start the server

echo "Starting xrdp server..."
killall -9 xrdp 2>/dev/null
sleep 1

/Applications/xrdp.app/Contents/Helpers/xrdp --nodaemon &
XRDP_PID=$!

echo "Waiting for server to start..."
sleep 2

if lsof -i :3389 > /dev/null 2>&1; then
    echo "✓ Server started successfully (PID: $XRDP_PID)"
    echo "✓ Listening on port 3389"
    echo ""
    echo "To test TLS 1.3 handshake:"
    echo "  python3 /tmp/test-full-tls13.py"
    echo ""
    echo "Server is running. Press Ctrl+C to stop."
    echo ""

    # Keep script running
    trap "kill $XRDP_PID 2>/dev/null; echo 'Server stopped'; exit 0" INT TERM
    wait $XRDP_PID
else
    echo "✗ Server failed to start"
    kill $XRDP_PID 2>/dev/null
    exit 1
fi
