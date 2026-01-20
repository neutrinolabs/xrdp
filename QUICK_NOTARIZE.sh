#!/bin/bash
#
# Quick Notarization Script for XRDP DMG
#
# This script will notarize XRDP-0.10.0-macOS.dmg
#
# FIRST TIME SETUP:
# 1. Get app-specific password from https://appleid.apple.com
# 2. Run this command ONCE to store credentials:
#
#    xcrun notarytool store-credentials "xrdp-notary" \
#      --apple-id xrdp@neutrinos.app \
#      --team-id F74NN74X3P
#
#    (It will prompt for the app-specific password)
#
# Then run this script: ./QUICK_NOTARIZE.sh
#

set -e

DMG_FILE="/Users/cyclic/xrdp/XRDP-0.10.0-macOS.dmg"
PROFILE="xrdp-notary"

echo "============================================"
echo "Notarizing XRDP DMG"
echo "============================================"
echo ""

# Check if DMG exists
if [ ! -f "$DMG_FILE" ]; then
    echo "ERROR: DMG not found at $DMG_FILE"
    exit 1
fi

# Check if signed
echo "Verifying code signature..."
codesign -dvv "$DMG_FILE" 2>&1 | grep -q "Authority=Developer ID"
if [ $? -ne 0 ]; then
    echo "ERROR: DMG is not signed with Developer ID"
    exit 1
fi
echo "✓ DMG is signed"
echo ""

# Submit for notarization
echo "[1/3] Submitting DMG to Apple notary service..."
echo "This typically takes 5-15 minutes..."
echo ""

xcrun notarytool submit "$DMG_FILE" \
    --keychain-profile "$PROFILE" \
    --wait

if [ $? -ne 0 ]; then
    echo ""
    echo "ERROR: Notarization failed"
    echo ""
    echo "To view the log, first get the submission ID from above, then run:"
    echo "xcrun notarytool log <submission-id> --keychain-profile $PROFILE"
    exit 1
fi

echo ""
echo "✓ Notarization successful!"
echo ""

# Staple the ticket
echo "[2/3] Stapling notarization ticket to DMG..."
xcrun stapler staple "$DMG_FILE"

if [ $? -eq 0 ]; then
    echo "✓ Ticket stapled to DMG"
else
    echo "WARNING: Failed to staple (DMG may still be valid)"
fi

echo ""

# Verify
echo "[3/3] Verifying notarization..."
spctl -a -vv -t install "$DMG_FILE"

echo ""
echo "============================================"
echo "Notarization Complete!"
echo "============================================"
echo ""
echo "The DMG is now notarized and ready for distribution."
echo ""
echo "Next steps:"
echo "1. Upload the notarized DMG to GitHub release"
echo "2. Users can download and install without warnings"
echo ""
echo "To upload to GitHub:"
echo ""
echo "  # Get asset ID"
echo "  ASSET_ID=\$(curl -s -H \"Authorization: token YOUR_TOKEN\" \\"
echo "    https://api.github.com/repos/Cyclic/xrdp/releases/278192327/assets | \\"
echo "    jq -r '.[] | select(.name==\"XRDP-0.10.0-macOS.dmg\") | .id')"
echo ""
echo "  # Delete old asset"
echo "  curl -X DELETE \\"
echo "    -H \"Authorization: token YOUR_TOKEN\" \\"
echo "    https://api.github.com/repos/Cyclic/xrdp/releases/assets/\${ASSET_ID}"
echo ""
echo "  # Upload notarized DMG"
echo "  curl -X POST \\"
echo "    -H \"Authorization: token YOUR_TOKEN\" \\"
echo "    -H \"Content-Type: application/x-apple-diskimage\" \\"
echo "    \"https://uploads.github.com/repos/Cyclic/xrdp/releases/278192327/assets?name=XRDP-0.10.0-macOS.dmg\" \\"
echo "    --data-binary \"@$DMG_FILE\""
echo ""
