#!/bin/bash
set -e

echo "==========================================="
echo "Notarizing XRDP with App Store Connect API"
echo "==========================================="

# Configuration
API_KEY_PATH="$HOME/Downloads/AuthKey_N5Q2ZKTJB5.p8"
API_KEY_ID="N5Q2ZKTJB5"
API_ISSUER_ID="e73e7c0f-7ade-4a3f-bf61-f9c176b84abc"
VERSION="0.10.0-macos-5-neutrinossl"

# Files to notarize
DIST_DIR="/Users/cyclic/xrdp/xrdp-macos-app/dist"
PKG_FILE="$DIST_DIR/XRDP-$VERSION-signed-clean.pkg"
DMG_FILE="$DIST_DIR/XRDP-$VERSION.dmg"

if [ ! -f "$API_KEY_PATH" ]; then
    echo "❌ Error: API key not found at $API_KEY_PATH"
    exit 1
fi

echo ""
echo "Using API Key: $API_KEY_ID"
echo "Issuer ID: $API_ISSUER_ID"
echo ""

# Notarize the signed package
echo "Step 1: Notarizing signed package..."
echo "  Uploading $PKG_FILE..."
xcrun notarytool submit "$PKG_FILE" \
    --key "$API_KEY_PATH" \
    --key-id "$API_KEY_ID" \
    --issuer "$API_ISSUER_ID" \
    --wait

echo ""
echo "Step 2: Stapling notarization ticket to package..."
xcrun stapler staple "$PKG_FILE"
echo "  ✅ Package notarized and stapled"

# Notarize the DMG
echo ""
echo "Step 3: Notarizing DMG..."
echo "  Uploading $DMG_FILE..."
xcrun notarytool submit "$DMG_FILE" \
    --key "$API_KEY_PATH" \
    --key-id "$API_KEY_ID" \
    --issuer "$API_ISSUER_ID" \
    --wait

echo ""
echo "Step 4: Stapling notarization ticket to DMG..."
xcrun stapler staple "$DMG_FILE"
echo "  ✅ DMG notarized and stapled"

echo ""
echo "==========================================="
echo "✅ All files notarized successfully!"
echo "==========================================="
echo ""
echo "Notarized files:"
echo "  - $PKG_FILE"
echo "  - $DMG_FILE"
echo ""
echo "Verify with:"
echo "  spctl --assess --type install --verbose=4 \"$PKG_FILE\""
echo "  spctl --assess --type open --context context:primary-signature --verbose=4 \"$DMG_FILE\""
