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
PKG_NAME="xrdp-${VERSION}-ard-macos-${ARCH}"
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

# Fix library paths - bundle OpenSSL and fix references
echo "Fixing library paths..."

# Define paths - detect OpenSSL location from built binary
LIB_DEST="$BUILD_DIR/root$INSTALL_PREFIX/lib/xrdp"

# Find where OpenSSL was linked from by checking the xrdp binary
OPENSSL_SRC=""
if [ -f "$BUILD_DIR/root$INSTALL_PREFIX/sbin/xrdp" ]; then
    OPENSSL_PATH=$(otool -L "$BUILD_DIR/root$INSTALL_PREFIX/sbin/xrdp" 2>/dev/null | grep libssl | awk '{print $1}' | head -1)
    if [ -n "$OPENSSL_PATH" ] && [ "$OPENSSL_PATH" != "/usr/lib/libssl"* ]; then
        OPENSSL_SRC=$(dirname "$OPENSSL_PATH")
    fi
fi

# Fallback to common locations if not detected
if [ -z "$OPENSSL_SRC" ] || [ ! -f "$OPENSSL_SRC/libssl.3.dylib" ]; then
    for dir in "$HOME/xrdp-deps/local/lib" "/opt/homebrew/opt/openssl@3/lib" "/usr/local/opt/openssl@3/lib"; do
        if [ -f "$dir/libssl.3.dylib" ]; then
            OPENSSL_SRC="$dir"
            break
        fi
    done
fi

echo "  OpenSSL source: ${OPENSSL_SRC:-not found}"

# Copy OpenSSL libraries to the package
if [ -f "$OPENSSL_SRC/libssl.3.dylib" ]; then
    cp "$OPENSSL_SRC/libssl.3.dylib" "$LIB_DEST/"
    cp "$OPENSSL_SRC/libcrypto.3.dylib" "$LIB_DEST/"
    chmod 755 "$LIB_DEST/libssl.3.dylib" "$LIB_DEST/libcrypto.3.dylib"

    # Fix the id of the bundled libraries
    install_name_tool -id "$INSTALL_PREFIX/lib/xrdp/libssl.3.dylib" "$LIB_DEST/libssl.3.dylib"
    install_name_tool -id "$INSTALL_PREFIX/lib/xrdp/libcrypto.3.dylib" "$LIB_DEST/libcrypto.3.dylib"

    # Fix libssl's reference to libcrypto
    install_name_tool -change "$OPENSSL_SRC/libcrypto.3.dylib" "$INSTALL_PREFIX/lib/xrdp/libcrypto.3.dylib" "$LIB_DEST/libssl.3.dylib"

    echo "  Bundled OpenSSL libraries"
else
    echo "WARNING: OpenSSL libraries not found at $OPENSSL_SRC"
    echo "         The package may not work on other systems!"
fi

# Fix all binaries and libraries that reference the hardcoded paths
fix_library_paths() {
    local file="$1"
    if [ -f "$file" ] && file "$file" | grep -q "Mach-O"; then
        # Check if it has the hardcoded path
        if otool -L "$file" 2>/dev/null | grep -q "$OPENSSL_SRC"; then
            install_name_tool -change "$OPENSSL_SRC/libssl.3.dylib" "$INSTALL_PREFIX/lib/xrdp/libssl.3.dylib" "$file" 2>/dev/null || true
            install_name_tool -change "$OPENSSL_SRC/libcrypto.3.dylib" "$INSTALL_PREFIX/lib/xrdp/libcrypto.3.dylib" "$file" 2>/dev/null || true
            echo "  Fixed: $(basename "$file")"
        fi
    fi
}

# Fix binaries
for bin in "$BUILD_DIR/root$INSTALL_PREFIX/sbin"/*; do
    fix_library_paths "$bin"
done

for bin in "$BUILD_DIR/root$INSTALL_PREFIX/bin"/*; do
    fix_library_paths "$bin"
done

# Fix libraries
for lib in "$BUILD_DIR/root$INSTALL_PREFIX/lib/xrdp"/*.dylib; do
    fix_library_paths "$lib"
done

# Also fix any .so files (module plugins)
for lib in "$BUILD_DIR/root$INSTALL_PREFIX/lib/xrdp"/*.so; do
    fix_library_paths "$lib"
done

echo "  Library path fixup complete"

# Code signing
echo "Signing binaries..."

# Detect signing identity
SIGNING_IDENTITY=$(security find-identity -v -p codesigning | grep "Developer ID Application" | head -1 | awk -F'"' '{print $2}' || echo "")

if [ -n "$SIGNING_IDENTITY" ]; then
    echo "  Using identity: $SIGNING_IDENTITY"

    # Sign all binaries
    for bin in "$BUILD_DIR/root$INSTALL_PREFIX/sbin"/* "$BUILD_DIR/root$INSTALL_PREFIX/bin"/* "$BUILD_DIR/root$INSTALL_PREFIX/libexec/xrdp"/*; do
        if [ -f "$bin" ] && file "$bin" | grep -q "Mach-O"; then
            codesign --force --sign "$SIGNING_IDENTITY" --timestamp --options runtime "$bin" 2>/dev/null && \
                echo "  Signed: $(basename "$bin")" || true
        fi
    done

    # Sign all libraries
    for lib in "$BUILD_DIR/root$INSTALL_PREFIX/lib/xrdp"/*.dylib "$BUILD_DIR/root$INSTALL_PREFIX/lib/xrdp"/*.so; do
        if [ -f "$lib" ] && file "$lib" | grep -q "Mach-O"; then
            codesign --force --sign "$SIGNING_IDENTITY" --timestamp "$lib" 2>/dev/null && \
                echo "  Signed: $(basename "$lib")" || true
        fi
    done

    echo "  Code signing complete"
else
    echo "  WARNING: No Developer ID certificate found - binaries will not be signed"
    echo "  Run: $SCRIPT_DIR/setup-signing.sh to create a certificate"
fi

# Copy additional files
echo "Adding macOS-specific files..."

# Create share directory for our files
mkdir -p "$BUILD_DIR/root$INSTALL_PREFIX/share/xrdp"
mkdir -p "$BUILD_DIR/root$INSTALL_PREFIX/share/xrdp/licenses"

# Copy license files
echo "  Adding license files..."

# Copy main xrdp license
cp "$XRDP_SRC/COPYING" "$BUILD_DIR/root$INSTALL_PREFIX/share/xrdp/licenses/XRDP-LICENSE.txt"

# Copy third-party licenses from xrdp repo
cp "$XRDP_SRC/third_party/COPYING-THIRD-PARTY" "$BUILD_DIR/root$INSTALL_PREFIX/share/xrdp/licenses/TOMLC99-LICENSE.txt"

# Add OpenSSL license (required when bundling OpenSSL libraries)
cat > "$BUILD_DIR/root$INSTALL_PREFIX/share/xrdp/licenses/OPENSSL-LICENSE.txt" << 'OPENSSL_EOF'
OpenSSL 3.x - Apache License 2.0

Copyright (c) OpenSSL Project Contributors
https://www.openssl.org/

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
OPENSSL_EOF

# Add NeutrinoRDP/FreeRDP license (Apache 2.0)
cat > "$BUILD_DIR/root$INSTALL_PREFIX/share/xrdp/licenses/NEUTRINORDP-LICENSE.txt" << 'FREERDP_EOF'
NeutrinoRDP (FreeRDP fork) - Apache License 2.0

Copyright 2011-2012 Vic Lee
Copyright 2011-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
FREERDP_EOF

# Create a combined NOTICE file
cat > "$BUILD_DIR/root$INSTALL_PREFIX/share/xrdp/licenses/NOTICE.txt" << 'NOTICE_EOF'
xrdp macOS Package - Third-Party Licenses
==========================================

This package contains the following third-party software:

1. xrdp - Apache License 2.0
   https://github.com/neutrinolabs/xrdp
   See: XRDP-LICENSE.txt

2. OpenSSL 3.x - Apache License 2.0
   https://www.openssl.org/
   See: OPENSSL-LICENSE.txt

3. NeutrinoRDP (FreeRDP fork) - Apache License 2.0
   https://github.com/neutrinolabs/NeutrinoRDP
   See: NEUTRINORDP-LICENSE.txt

4. tomlc99 - MIT License
   https://github.com/cktan/tomlc99
   See: TOMLC99-LICENSE.txt

All licenses are included in this directory.
NOTICE_EOF

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
UNSIGNED_PKG="$BUILD_DIR/$PKG_NAME-unsigned.pkg"
productbuild \
    --distribution "$BUILD_DIR/distribution.xml" \
    --resources "$BUILD_DIR/resources" \
    --package-path "$BUILD_DIR" \
    "$UNSIGNED_PKG"

# Sign the package
INSTALLER_IDENTITY=$(security find-identity -v -p basic | grep "Developer ID Installer" | head -1 | awk -F'"' '{print $2}' || echo "")
if [ -n "$INSTALLER_IDENTITY" ]; then
    echo "Signing package..."
    echo "  Using identity: $INSTALLER_IDENTITY"
    productsign --sign "$INSTALLER_IDENTITY" --timestamp "$UNSIGNED_PKG" "$SCRIPT_DIR/$PKG_NAME.pkg"
    echo "  Package signed successfully"
else
    # No signing identity, just move unsigned package
    mv "$UNSIGNED_PKG" "$SCRIPT_DIR/$PKG_NAME.pkg"
    echo "  WARNING: Package is unsigned"
fi

echo ""
echo "============================================"
echo "Package built successfully!"
echo "============================================"
echo ""
echo "Output: $SCRIPT_DIR/$PKG_NAME.pkg"
echo ""

# Optional: Notarize the package
if [ "$NOTARIZE" = "yes" ]; then
    echo "Notarizing package with Apple..."
    "$SCRIPT_DIR/notarize-pkg.sh" "$SCRIPT_DIR/$PKG_NAME.pkg"
    if [ $? -eq 0 ]; then
        echo ""
        echo "✓ Package is notarized and ready for distribution"
    else
        echo ""
        echo "WARNING: Notarization failed. Package is signed but not notarized."
        echo "Run: $SCRIPT_DIR/notarize-pkg.sh $SCRIPT_DIR/$PKG_NAME.pkg"
    fi
fi

echo ""
echo "To install: sudo installer -pkg $SCRIPT_DIR/$PKG_NAME.pkg -target /"
echo ""

# Clean up
rm -rf "$BUILD_DIR"

exit 0
