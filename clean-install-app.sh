#!/bin/bash
# Clean install xrdp.app to /Applications

set -e

echo "=== Clean Install xrdp.app ==="

# Kill any running instances
echo "Stopping xrdp processes..."
killall xrdp xrdp-sesman xrdp-chansrv 2>/dev/null || true
sleep 1

# Remove old app - try with sudo if needed for root-owned files
if [ -d "/Applications/xrdp.app" ]; then
    echo "Removing old installation..."
    if rm -rf /Applications/xrdp.app 2>/dev/null; then
        echo "Removed without sudo"
    else
        echo "Need sudo to remove root-owned files..."
        sudo rm -rf /Applications/xrdp.app
    fi
fi

# Copy new app
echo "Installing new build..."
cp -R build/Debug/xrdp.app /Applications/

# Fix permissions
sudo chown -R $(whoami):staff /Applications/xrdp.app 2>/dev/null || chown -R $(whoami):staff /Applications/xrdp.app

echo "✅ Installation complete"
echo "Launch with: open /Applications/xrdp.app"
