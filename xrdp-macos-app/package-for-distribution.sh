#!/bin/bash
set -e

echo "==========================================="
echo "Packaging XRDP for macOS Distribution"
echo "==========================================="

# Configuration
APP_NAME="XRDP"
BUNDLE_ID="remotex.app"
VERSION="0.10.0-macos-5-neutrinossl"
DEVELOPER_ID_APP="Developer ID Application: Neutrinos Platforms, Inc. (H4PF9B4P9G)"
DEVELOPER_ID_INSTALLER="Developer ID Installer: Neutrinos Platforms, Inc. (H4PF9B4P9G)"
TEAM_ID="H4PF9B4P9G"
APPLE_ID="tgoddard@neutrinos.co"

# Directories
DIST_DIR="/Users/cyclic/xrdp/xrdp-macos-app/dist"
SOURCE_APP="/Applications/XRDP.app"
DIST_APP="$DIST_DIR/$APP_NAME.app"
PKG_FILE="$DIST_DIR/$APP_NAME-$VERSION.pkg"
DMG_FILE="$DIST_DIR/$APP_NAME-$VERSION.dmg"

rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR"

echo ""
echo "Step 1: Copying app from /Applications..."
cp -R "$SOURCE_APP" "$DIST_APP"

echo ""
echo "Step 2: Re-signing app bundle with hardened runtime for distribution..."
# Sign all libraries
for lib in "$DIST_APP/Contents/Resources/lib/xrdp"/*.dylib; do
    if [ -f "$lib" ]; then
        echo "  Signing $(basename $lib)..."
        codesign --force --sign "$DEVELOPER_ID_APP" \
            --timestamp \
            --options runtime \
            "$lib" 2>&1 | grep -v "replacing existing signature" || true
    fi
done

# Sign helpers
for helper in "$DIST_APP/Contents/Helpers"/*; do
    if [ -f "$helper" ]; then
        echo "  Signing $(basename $helper)..."
        codesign --force --sign "$DEVELOPER_ID_APP" \
            --timestamp \
            --options runtime \
            --entitlements "/Users/cyclic/xrdp/xrdp-macos-app/xrdp.entitlements" \
            "$helper" 2>&1 | grep -v "replacing existing signature" || true
    fi
done

# Sign the main app bundle
echo "  Signing app bundle..."
codesign --force --deep --sign "$DEVELOPER_ID_APP" \
    --timestamp \
    --options runtime \
    --entitlements "/Users/cyclic/xrdp/xrdp-macos-app/xrdp.entitlements" \
    "$DIST_APP"

echo ""
echo "Step 3: Verifying signature..."
codesign --verify --deep --strict --verbose=2 "$DIST_APP"

echo ""
echo "Step 4: Creating installer package..."
pkgbuild --root "$DIST_DIR" \
    --identifier "$BUNDLE_ID" \
    --version "$VERSION" \
    --install-location /Applications \
    --sign "$DEVELOPER_ID_INSTALLER" \
    "$PKG_FILE"

echo ""
echo "Step 5: Notarizing package..."
echo "Uploading to Apple for notarization..."
xcrun notarytool submit "$PKG_FILE" \
    --apple-id "$APPLE_ID" \
    --team-id "$TEAM_ID" \
    --wait

echo ""
echo "Step 6: Stapling notarization ticket..."
xcrun stapler staple "$PKG_FILE"

echo ""
echo "Step 7: Creating DMG..."
hdiutil create -volname "$APP_NAME" \
    -srcfolder "$DIST_APP" \
    -ov -format UDZO \
    "$DMG_FILE"

echo ""
echo "Step 8: Signing DMG..."
codesign --force --sign "$DEVELOPER_ID_APP" \
    --timestamp \
    "$DMG_FILE"

echo ""
echo "Step 9: Notarizing DMG..."
xcrun notarytool submit "$DMG_FILE" \
    --apple-id "$APPLE_ID" \
    --team-id "$TEAM_ID" \
    --wait

xcrun stapler staple "$DMG_FILE"

echo ""
echo "✅ Distribution package complete!"
echo ""
echo "Files created:"
echo "  - $DIST_APP"
echo "  - $PKG_FILE"
echo "  - $DMG_FILE"
echo ""
echo "All files are signed and notarized for distribution."
