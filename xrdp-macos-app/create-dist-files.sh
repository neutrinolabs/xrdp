#!/bin/bash
set -e

echo "==========================================="
echo "Creating XRDP Distribution Files"
echo "==========================================="

# Configuration
APP_NAME="XRDP"
BUNDLE_ID="remotex.app"
VERSION="0.10.0-macos-5-neutrinossl"
DEVELOPER_ID_APP="Developer ID Application: Neutrinos Platforms, Inc. (H4PF9B4P9G)"
DEVELOPER_ID_INSTALLER="Developer ID Installer: Neutrinos Platforms, Inc. (H4PF9B4P9G)"

# Directories
DIST_DIR="/Users/cyclic/xrdp/xrdp-macos-app/dist"
SOURCE_APP="/Applications/XRDP.app"
DIST_APP="$DIST_DIR/$APP_NAME.app"
PKG_FILE="$DIST_DIR/$APP_NAME-$VERSION.pkg"
SIGNED_PKG_FILE="$DIST_DIR/$APP_NAME-$VERSION-signed.pkg"
DMG_FILE="$DIST_DIR/$APP_NAME-$VERSION.dmg"
ISO_FILE="$DIST_DIR/$APP_NAME-$VERSION.iso"

rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR"

echo ""
echo "Step 1: Copying and signing app..."
cp -R "$SOURCE_APP" "$DIST_APP"

# Sign all libraries with hardened runtime
echo "  Signing libraries..."
for lib in "$DIST_APP/Contents/Resources/lib/xrdp"/*.dylib; do
    [ -f "$lib" ] && codesign --force --sign "$DEVELOPER_ID_APP" --timestamp --options runtime "$lib" 2>/dev/null || true
done

# Sign helpers with entitlements
echo "  Signing helpers..."
for helper in "$DIST_APP/Contents/Helpers"/*; do
    [ -f "$helper" ] && codesign --force --sign "$DEVELOPER_ID_APP" --timestamp --options runtime --entitlements "/Users/cyclic/xrdp/xrdp-macos-app/xrdp.entitlements" "$helper" 2>/dev/null || true
done

# Sign the main app bundle
echo "  Signing app bundle..."
codesign --force --deep --sign "$DEVELOPER_ID_APP" --timestamp --options runtime --entitlements "/Users/cyclic/xrdp/xrdp-macos-app/xrdp.entitlements" "$DIST_APP"

echo "  ✅ App signed successfully"

echo ""
echo "Step 2: Creating installer package..."
pkgbuild --root "$DIST_DIR" \
    --identifier "$BUNDLE_ID" \
    --version "$VERSION" \
    --install-location /Applications \
    "$PKG_FILE"

echo "  ✅ Unsigned package created: $PKG_FILE"

# Create signed version
pkgbuild --root "$DIST_DIR" \
    --identifier "$BUNDLE_ID" \
    --version "$VERSION" \
    --install-location /Applications \
    --sign "$DEVELOPER_ID_INSTALLER" \
    "$SIGNED_PKG_FILE"

echo "  ✅ Signed package created: $SIGNED_PKG_FILE"

echo ""
echo "Step 3: Creating DMG..."
hdiutil create -volname "$APP_NAME" \
    -srcfolder "$DIST_APP" \
    -ov -format UDZO \
    "$DMG_FILE"

# Sign the DMG
codesign --force --sign "$DEVELOPER_ID_APP" --timestamp "$DMG_FILE"

echo "  ✅ DMG created and signed: $DMG_FILE"

echo ""
echo "Step 4: Creating ISO..."
hdiutil create -volname "$APP_NAME" \
    -srcfolder "$DIST_APP" \
    -ov -format UDTO \
    "$ISO_FILE"

echo "  ✅ ISO created: $ISO_FILE"

echo ""
echo "Step 5: Generating checksums..."
cd "$DIST_DIR"
shasum -a 256 *.pkg *.dmg *.iso > SHA256SUMS.txt

echo ""
echo "==========================================="
echo "✅ Distribution files created successfully!"
echo "==========================================="
echo ""
ls -lh "$DIST_DIR"
echo ""
echo "Files:"
echo "  1. $APP_NAME.app - Signed app bundle"
echo "  2. $APP_NAME-$VERSION.pkg - Unsigned installer"
echo "  3. $APP_NAME-$VERSION-signed.pkg - Signed installer"
echo "  4. $APP_NAME-$VERSION.dmg - Signed disk image"
echo "  5. $APP_NAME-$VERSION.iso - ISO image"
echo "  6. SHA256SUMS.txt - Checksums"
echo ""
echo "To notarize (requires app-specific password):"
echo "  xcrun notarytool submit $SIGNED_PKG_FILE --apple-id YOUR_APPLE_ID --team-id H4PF9B4P9G --password @keychain:AC_PASSWORD --wait"
echo "  xcrun stapler staple $SIGNED_PKG_FILE"
echo ""
