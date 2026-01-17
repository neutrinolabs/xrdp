#!/bin/bash
# Disable system xrdp daemons that conflict with the GUI app

echo "Disabling system xrdp daemons..."

# Unload the launchd services
sudo launchctl unload /Library/LaunchDaemons/com.xrdp.xrdp.plist 2>/dev/null
sudo launchctl unload /Library/LaunchDaemons/com.xrdp.sesman.plist 2>/dev/null

# Kill any remaining processes
sudo pkill -f "/usr/local/sbin/xrdp"
sudo pkill -f "/usr/local/sbin/xrdp-sesman"

echo "Done. Checking if port 3389 is free..."
lsof -nP -iTCP:3389 -sTCP:LISTEN

if [ $? -ne 0 ]; then
    echo "Port 3389 is now free!"
else
    echo "WARNING: Port 3389 is still in use"
fi
