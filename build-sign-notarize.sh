#!/bin/bash
#
# Complete Build, Sign, and Notarize Workflow for XRDP macOS
#
# This script:
# 1. Builds XRDP.app with the latest code
# 2. Signs the app bundle with Developer ID
# 3. Creates and signs the DMG
# 4. Notarizes both app and DMG
# 5. Staples notarization tickets
# 6. Verifies everything is ready for distribution
#
# PREREQUISITES:
# - Xcode command line tools installed
# - Developer ID Application certificate in keychain
# - App Store Connect API key at /Users/cyclic/Downloads/AuthKey_N5Q2ZKTJB5.p8
# - ISSUER_ID environment variable set (get from App Store Connect)
#
# FIRST TIME SETUP - Get Issuer ID:
#   1. Visit https://appstoreconnect.apple.com/access/integrations/api
#   2. Login with xrdp@neutrinos.app
#   3. Copy the Issuer ID (UUID format)
#   4. Run: export ISSUER_ID="paste-uuid-here"
#   5. Add to ~/.zshrc to make permanent
#
# USAGE:
#   ./build-sign-notarize.sh
#

set -e

# Source environment variables
if [ -f ~/.zshrc ]; then
    source ~/.zshrc 2>/dev/null || true
fi
if [ -f ~/.zprofile ]; then
    source ~/.zprofile 2>/dev/null || true
fi

# Configuration
APP_NAME="XRDP"
BUNDLE_ID="remotex.app"
TEAM_ID="H4PF9B4P9G"
SIGNING_IDENTITY="Developer ID Application: Neutrinos Platforms, Inc. (H4PF9B4P9G)"
API_KEY_PATH="/Users/cyclic/Downloads/AuthKey_N5Q2ZKTJB5.p8"
API_KEY_ID="N5Q2ZKTJB5"
VERSION="0.10.0"
DMG_NAME="XRDP-${VERSION}-macOS.dmg"

# Paths
XCODE_PROJECT_DIR="/Users/cyclic/xrdp/xrdp-macos-app"
BUILD_DIR="$XCODE_PROJECT_DIR/build/Build/Products/Debug"
APP_PATH="$BUILD_DIR/$APP_NAME.app"
DIST_DIR="/Users/cyclic/xrdp"
DMG_PATH="$DIST_DIR/$DMG_NAME"
ZIP_PATH="$DIST_DIR/$APP_NAME.app.zip"

echo "========================================="
echo "XRDP Build, Sign & Notarize Workflow"
echo "========================================="
echo ""

# Check prerequisites
echo "[Pre-flight] Checking prerequisites..."
if [ ! -f "$API_KEY_PATH" ]; then
    echo "❌ ERROR: API key not found at $API_KEY_PATH"
    exit 1
fi

if [ -z "$ISSUER_ID" ]; then
    echo "❌ ERROR: ISSUER_ID environment variable not set"
    echo ""
    echo "Get your Issuer ID:"
    echo "  1. Visit https://appstoreconnect.apple.com/access/integrations/api"
    echo "  2. Login with xrdp@neutrinos.app"
    echo "  3. Copy the Issuer ID (UUID at top of page)"
    echo "  4. Run: export ISSUER_ID='your-uuid-here'"
    echo ""
    exit 1
fi

security find-identity -v -p codesigning | grep -q "$TEAM_ID"
if [ $? -ne 0 ]; then
    echo "❌ ERROR: Signing identity not found in keychain"
    echo "Expected: $SIGNING_IDENTITY"
    exit 1
fi

echo "✅ All prerequisites met"
echo ""

# Step 1: Build the app
echo "[1/9] Building XRDP.app..."
cd "$XCODE_PROJECT_DIR"

# Build (ignore sandbox errors in patch script - they're not critical)
xcodebuild -scheme xrdp -configuration Debug -derivedDataPath build clean build 2>&1 | \
    grep -v "sandbox-exec" | \
    grep -E "(BUILD|Compiling|Linking|Processing|SUCCEEDED|FAILED)" | \
    tail -20

if [ ! -d "$APP_PATH" ]; then
    echo "❌ ERROR: Build failed - app not found at $APP_PATH"
    exit 1
fi

APP_SIZE=$(du -sh "$APP_PATH" | cut -f1)
echo "✅ Built: $APP_PATH ($APP_SIZE)"
echo ""

# Step 2: Sign the app bundle
echo "[2/9] Signing app bundle..."
codesign --force --deep \
    --sign "$SIGNING_IDENTITY" \
    --timestamp \
    --options runtime \
    --entitlements "$XCODE_PROJECT_DIR/xrdp.entitlements" \
    "$APP_PATH"

# Verify signature
codesign -dvvv "$APP_PATH" 2>&1 | grep -q "Developer ID Application"
if [ $? -ne 0 ]; then
    echo "❌ ERROR: Code signing failed"
    exit 1
fi

echo "✅ App signed with Developer ID"
codesign -dvvv "$APP_PATH" 2>&1 | grep -E "(Identifier|Authority|Timestamp)" | head -4
echo ""

# Step 3: Create app zip for notarization
echo "[3/9] Creating app zip archive..."
cd "$DIST_DIR"
rm -f "$ZIP_PATH"
ditto -c -k --keepParent "$APP_PATH" "$ZIP_PATH"
ZIP_SIZE=$(du -sh "$ZIP_PATH" | cut -f1)
echo "✅ Created: $ZIP_PATH ($ZIP_SIZE)"
echo ""

# Step 4: Notarize the app
echo "[4/9] Submitting app for notarization..."
echo "This may take 5-15 minutes..."
echo ""

xcrun notarytool submit "$ZIP_PATH" \
    --key "$API_KEY_PATH" \
    --key-id "$API_KEY_ID" \
    --issuer "$ISSUER_ID" \
    --wait

if [ $? -ne 0 ]; then
    echo "❌ ERROR: App notarization failed"
    echo "Check the log above for details"
    exit 1
fi

echo ""
echo "✅ App notarization successful!"
echo ""

# Step 5: Staple to app (extract from zip first)
echo "[5/9] Stapling notarization ticket to app..."
xcrun stapler staple "$APP_PATH"
if [ $? -eq 0 ]; then
    echo "✅ Ticket stapled to app"
else
    echo "⚠️  Warning: Failed to staple to app (not critical)"
fi
echo ""

# Step 6: Create DMG
echo "[6/9] Creating DMG installer..."

# Remove old DMG if exists
[ -f "$DMG_PATH" ] && rm -f "$DMG_PATH"

# Create temporary directory for DMG contents
TMP_DIR=$(mktemp -d)
cp -R "$APP_PATH" "$TMP_DIR/"

# Create DMG
hdiutil create -volname "XRDP $VERSION" \
    -srcfolder "$TMP_DIR" \
    -ov -format UDZO \
    "$DMG_PATH"

# Cleanup
rm -rf "$TMP_DIR"

DMG_SIZE=$(du -sh "$DMG_PATH" | cut -f1)
echo "✅ Created: $DMG_PATH ($DMG_SIZE)"
echo ""

# Step 7: Sign the DMG
echo "[7/9] Signing DMG..."
codesign --force \
    --sign "$SIGNING_IDENTITY" \
    --timestamp \
    "$DMG_PATH"

codesign -dvvv "$DMG_PATH" 2>&1 | grep -q "Developer ID Application"
if [ $? -ne 0 ]; then
    echo "❌ ERROR: DMG signing failed"
    exit 1
fi

echo "✅ DMG signed"
echo ""

# Step 8: Notarize the DMG
echo "[8/9] Submitting DMG for notarization..."
echo "This may take another 5-15 minutes..."
echo ""

xcrun notarytool submit "$DMG_PATH" \
    --key "$API_KEY_PATH" \
    --key-id "$API_KEY_ID" \
    --issuer "$ISSUER_ID" \
    --wait

if [ $? -ne 0 ]; then
    echo "❌ ERROR: DMG notarization failed"
    exit 1
fi

echo ""
echo "✅ DMG notarization successful!"
echo ""

# Step 9: Staple to DMG
echo "[9/9] Stapling notarization ticket to DMG..."
xcrun stapler staple "$DMG_PATH"
if [ $? -eq 0 ]; then
    echo "✅ Ticket stapled to DMG"
else
    echo "⚠️  Warning: Failed to staple (not critical)"
fi
echo ""

# Final verification
echo "========================================="
echo "Verifying notarization..."
echo "========================================="
echo ""

spctl -a -vv -t install "$DMG_PATH"

echo ""
echo "========================================="
echo "🎉 SUCCESS!"
echo "========================================="
echo ""
echo "Files ready for distribution:"
echo "  • App:  $APP_PATH"
echo "  • Zip:  $ZIP_PATH"
echo "  • DMG:  $DMG_PATH"
echo ""
echo "DMG Details:"
du -h "$DMG_PATH"
shasum -a 256 "$DMG_PATH"
echo ""
echo "Next steps:"
echo "  1. Test the DMG on a clean macOS system"
echo "  2. Upload to GitHub release:"
echo "     gh release upload v${VERSION}-macos \"$DMG_PATH\" --clobber"
echo ""
