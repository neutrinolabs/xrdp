#!/bin/bash
# Check system daemon logs (requires sudo)

echo "=== Recent system xrdp daemon logs ==="
echo "Last 50 lines from /var/log/xrdp.log:"
echo ""
sudo tail -50 /var/log/xrdp.log

echo ""
echo "=== Recent connections ==="
sudo grep "connection accepted" /var/log/xrdp.log | tail -20

echo ""
echo "=== Recent errors ==="
sudo grep "ERROR" /var/log/xrdp.log | tail -20
