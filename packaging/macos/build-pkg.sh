#!/bin/bash
#
# Build macOS .pkg installer for xrdp with ARD support
#
# Usage: ./build-pkg.sh [version]
# Example: ./build-pkg.sh 0.10.0
#

set -e

VERSION="${1:-0.10.0}"
ARCH=$(uname -m)  # arm64 or x86_64
PKG_NAME="xrdp-${VERSION}-macos-${ARCH}"
BUILD_DIR="/tmp/xrdp-pkg-build"
INSTALL_PREFIX="/usr/local"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
XRDP_SRC="$(cd "$SCRIPT_DIR/../.." && pwd)"

echo "============================================"
echo "Building xrdp $VERSION for macOS $ARCH"
echo "============================================"
echo ""

# Clean up previous build
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR/root"
mkdir -p "$BUILD_DIR/scripts"
mkdir -p "$BUILD_DIR/resources"

# Build xrdp from source if not already built
if [ ! -f "$XRDP_SRC/xrdp/xrdp" ]; then
    echo "Building xrdp from source..."
    cd "$XRDP_SRC"

    if [ ! -f "configure" ]; then
        ./bootstrap
    fi

    ./configure --prefix="$INSTALL_PREFIX" \
        --enable-vsock \
        --disable-pam \
        --with-socketdir=/var/run/xrdp

    make -j$(sysctl -n hw.ncpu)
fi

# Install to staging directory
echo "Installing to staging directory..."
cd "$XRDP_SRC"
make DESTDIR="$BUILD_DIR/root" install

# Copy additional files
echo "Adding macOS-specific files..."

# Create share directory for our files
mkdir -p "$BUILD_DIR/root$INSTALL_PREFIX/share/xrdp"

# Copy launchd plists
cp "$SCRIPT_DIR/com.xrdp.xrdp.plist" "$BUILD_DIR/root$INSTALL_PREFIX/share/xrdp/"
cp "$SCRIPT_DIR/com.xrdp.sesman.plist" "$BUILD_DIR/root$INSTALL_PREFIX/share/xrdp/"

# Copy fix-screen-recording.sh
if [ -f "$XRDP_SRC/docs/macos/fix-screen-recording.sh" ]; then
    cp "$XRDP_SRC/docs/macos/fix-screen-recording.sh" "$BUILD_DIR/root$INSTALL_PREFIX/share/xrdp/"
    chmod +x "$BUILD_DIR/root$INSTALL_PREFIX/share/xrdp/fix-screen-recording.sh"
fi

# Copy test script
if [ -f "$XRDP_SRC/docs/macos/test_vnc_pixels.py" ]; then
    cp "$XRDP_SRC/docs/macos/test_vnc_pixels.py" "$BUILD_DIR/root$INSTALL_PREFIX/share/xrdp/"
fi

# Copy README
if [ -f "$XRDP_SRC/docs/macos/README.md" ]; then
    cp "$XRDP_SRC/docs/macos/README.md" "$BUILD_DIR/root$INSTALL_PREFIX/share/xrdp/"
fi

# Copy install scripts
cp "$SCRIPT_DIR/preinstall" "$BUILD_DIR/scripts/"
cp "$SCRIPT_DIR/postinstall" "$BUILD_DIR/scripts/"
chmod +x "$BUILD_DIR/scripts/preinstall"
chmod +x "$BUILD_DIR/scripts/postinstall"

# Create welcome message
cat > "$BUILD_DIR/resources/welcome.html" << 'EOF'
<!DOCTYPE html>
<html>
<head>
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, sans-serif; padding: 20px; }
        h1 { color: #333; }
        .note { background: #fff3cd; padding: 15px; border-radius: 8px; margin: 20px 0; }
    </style>
</head>
<body>
    <h1>xrdp for macOS</h1>
    <p>This package installs xrdp with Apple Remote Desktop (ARD) authentication support, allowing you to connect to your Mac using any RDP client.</p>

    <div class="note">
        <strong>Important:</strong> After installation, you'll need to:
        <ol>
            <li>Enable Screen Sharing in System Settings</li>
            <li>Disable SIP temporarily to grant permissions</li>
            <li>Run the provided setup script</li>
        </ol>
        <p>See the README at /usr/local/share/xrdp/README.md for detailed instructions.</p>
    </div>
</body>
</html>
EOF

# Create conclusion message
cat > "$BUILD_DIR/resources/conclusion.html" << 'EOF'
<!DOCTYPE html>
<html>
<head>
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, sans-serif; padding: 20px; }
        h1 { color: #28a745; }
        code { background: #f4f4f4; padding: 2px 6px; border-radius: 4px; }
        .steps { background: #e7f5ff; padding: 15px; border-radius: 8px; margin: 20px 0; }
    </style>
</head>
<body>
    <h1>Installation Complete!</h1>
    <p>xrdp has been installed and configured to start automatically.</p>

    <div class="steps">
        <strong>Next Steps:</strong>
        <ol>
            <li>Enable Screen Sharing: System Settings → General → Sharing → Screen Sharing</li>
            <li>Boot to Recovery Mode and run: <code>csrutil disable</code></li>
            <li>Reboot and run: <code>sudo /usr/local/share/xrdp/fix-screen-recording.sh</code></li>
            <li>Reboot one more time</li>
            <li>Connect using any RDP client to port 3389</li>
        </ol>
    </div>

    <p>For detailed instructions, see: <code>/usr/local/share/xrdp/README.md</code></p>
</body>
</html>
EOF

# Build component package
echo "Building component package..."
pkgbuild \
    --root "$BUILD_DIR/root" \
    --scripts "$BUILD_DIR/scripts" \
    --identifier "com.xrdp.xrdp" \
    --version "$VERSION" \
    --install-location "/" \
    "$BUILD_DIR/xrdp-component.pkg"

# Create distribution.xml
cat > "$BUILD_DIR/distribution.xml" << EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>xrdp $VERSION</title>
    <organization>com.xrdp</organization>
    <domains enable_localSystem="true"/>
    <options customize="never" require-scripts="true" rootVolumeOnly="true"/>

    <welcome file="welcome.html"/>
    <conclusion file="conclusion.html"/>

    <choices-outline>
        <line choice="default">
            <line choice="com.xrdp.xrdp"/>
        </line>
    </choices-outline>

    <choice id="default"/>
    <choice id="com.xrdp.xrdp" visible="false">
        <pkg-ref id="com.xrdp.xrdp"/>
    </choice>

    <pkg-ref id="com.xrdp.xrdp" version="$VERSION" onConclusion="none">xrdp-component.pkg</pkg-ref>
</installer-gui-script>
EOF

# Build product archive
echo "Building final installer package..."
productbuild \
    --distribution "$BUILD_DIR/distribution.xml" \
    --resources "$BUILD_DIR/resources" \
    --package-path "$BUILD_DIR" \
    "$SCRIPT_DIR/$PKG_NAME.pkg"

echo ""
echo "============================================"
echo "Package built successfully!"
echo "============================================"
echo ""
echo "Output: $SCRIPT_DIR/$PKG_NAME.pkg"
echo ""
echo "To install: sudo installer -pkg $SCRIPT_DIR/$PKG_NAME.pkg -target /"
echo ""

# Clean up
rm -rf "$BUILD_DIR"

exit 0
