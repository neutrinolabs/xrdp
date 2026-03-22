#!/bin/bash
set -e

echo "==========================================="
echo "Notarizing XRDP Release Files"
echo "==========================================="

# Configuration from environment
APPLE_ID="${APPLE_ID:-xrdp@neutrinos.app}"
TEAM_ID="H4PF9B4P9G"
VERSION="0.10.0-macos-5-neutrinossl"

# Get password from keychain
AC_PASSWORD=$(security find-generic-password -a "$APPLE_ID" -w 2>/dev/null)

if [ -z "$AC_PASSWORD" ]; then
    echo "❌ Error: Could not find notarization password in keychain"
    exit 1
fi

# Files to notarize
DIST_DIR="/Users/cyclic/xrdp/xrdp-macos-app/dist"
PKG_FILE="$DIST_DIR/XRDP-$VERSION-signed.pkg"
DMG_FILE="$DIST_DIR/XRDP-$VERSION.dmg"

echo ""
echo "Using Apple ID: $APPLE_ID"
echo "Team ID: $TEAM_ID"
echo ""

# Notarize the signed package
echo "Step 1: Notarizing signed package..."
echo "  Uploading $PKG_FILE..."
xcrun notarytool submit "$PKG_FILE" \
    --apple-id "$APPLE_ID" \
    --password "$AC_PASSWORD" \
    --team-id "$TEAM_ID" \
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
    --apple-id "$APPLE_ID" \
    --password "$AC_PASSWORD" \
    --team-id "$TEAM_ID" \
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
