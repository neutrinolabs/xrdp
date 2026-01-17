#!/bin/bash
# Install Debug xrdp.app to /Applications for testing

echo "Installing xrdp.app to /Applications..."

# Kill any running instances
killall xrdp xrdp-sesman 2>/dev/null || true
sleep 1

# Remove old app (requires sudo because of runtime files)
if [ -d "/Applications/xrdp.app" ]; then
    echo "Removing old installation..."
    sudo rm -rf /Applications/xrdp.app
fi

# Copy new app
echo "Copying Debug build..."
cp -R xrdp-macos-app/build/Debug/xrdp.app /Applications/

# Fix permissions
sudo chown -R $(whoami):staff /Applications/xrdp.app

echo "✅ Installation complete"
echo "Launch with: open /Applications/xrdp.app"
