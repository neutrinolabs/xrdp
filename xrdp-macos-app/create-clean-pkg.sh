#!/bin/bash
set -e

echo "==========================================="
echo "Creating Clean XRDP Package for Notarization"
echo "==========================================="

# Configuration
APP_NAME="XRDP"
BUNDLE_ID="remotex.app"
VERSION="0.10.0-macos-5-neutrinossl"
DEVELOPER_ID_INSTALLER="Developer ID Installer: Neutrinos Platforms, Inc. (H4PF9B4P9G)"

# Directories
DIST_DIR="/Users/cyclic/xrdp/xrdp-macos-app/dist"
APP_PATH="$DIST_DIR/$APP_NAME.app"
PAYLOAD_DIR="/tmp/xrdp-pkg-payload"
PKG_FILE="$DIST_DIR/$APP_NAME-$VERSION-signed-clean.pkg"

# Clean up
rm -rf "$PAYLOAD_DIR"
rm -f "$PKG_FILE"

# Create payload directory structure
echo "Creating package payload..."
mkdir -p "$PAYLOAD_DIR/Applications"

# Copy only the app (not the other packages)
echo "Copying $APP_NAME.app..."
cp -R "$APP_PATH" "$PAYLOAD_DIR/Applications/"

# Create the package
echo "Building package..."
pkgbuild --root "$PAYLOAD_DIR" \
    --identifier "$BUNDLE_ID" \
    --version "$VERSION" \
    --install-location / \
    --sign "$DEVELOPER_ID_INSTALLER" \
    "$PKG_FILE"

# Clean up
rm -rf "$PAYLOAD_DIR"

echo ""
echo "✅ Clean package created: $PKG_FILE"
ls -lh "$PKG_FILE"
echo ""
echo "Package contents:"
pkgutil --payload-files "$PKG_FILE" | head -20
