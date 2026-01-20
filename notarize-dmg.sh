#!/bin/bash
#
# Notarize XRDP DMG using App Store Connect API Key
#
# Usage:
#   ./notarize-dmg.sh [issuer-id]
#
# Or set environment variable:
#   export ISSUER_ID="your-uuid-here"
#   ./notarize-dmg.sh
#

set -e

DMG_FILE="/Users/cyclic/xrdp/XRDP-0.10.0-macOS.dmg"
API_KEY_PATH="/Users/cyclic/Downloads/AuthKey_N5Q2ZKTJB5.p8"
API_KEY_ID="N5Q2ZKTJB5"
ISSUER_ID="${1:-${ISSUER_ID:-}}"

echo "========================================"
echo "XRDP DMG Notarization"
echo "========================================"
echo ""

# Check if DMG exists
if [ ! -f "$DMG_FILE" ]; then
    echo "ERROR: DMG not found at $DMG_FILE"
    exit 1
fi

# Check if API key exists
if [ ! -f "$API_KEY_PATH" ]; then
    echo "ERROR: API key not found at $API_KEY_PATH"
    exit 1
fi

# Check if Issuer ID is provided
if [ -z "$ISSUER_ID" ]; then
    echo "ERROR: Issuer ID not provided"
    echo ""
    echo "Get your Issuer ID from:"
    echo "  https://appstoreconnect.apple.com/access/integrations/api"
    echo ""
    echo "Then run one of:"
    echo "  ./notarize-dmg.sh <issuer-id>"
    echo "  export ISSUER_ID='<issuer-id>' && ./notarize-dmg.sh"
    echo ""
    echo "Or run ./get-issuer-id.sh for detailed instructions"
    exit 1
fi

# Verify DMG signature
echo "[1/4] Verifying code signature..."
codesign -dvv "$DMG_FILE" 2>&1 | grep -q "Authority="
if [ $? -ne 0 ]; then
    echo "ERROR: DMG is not signed"
    exit 1
fi
echo "✓ DMG is signed"
echo ""

# Submit for notarization
echo "[2/4] Submitting to Apple notary service..."
echo "Using API Key: $API_KEY_ID"
echo "Issuer ID: $ISSUER_ID"
echo ""
echo "This may take 5-15 minutes..."
echo ""

xcrun notarytool submit "$DMG_FILE" \
    --key "$API_KEY_PATH" \
    --key-id "$API_KEY_ID" \
    --issuer "$ISSUER_ID" \
    --wait

if [ $? -ne 0 ]; then
    echo ""
    echo "ERROR: Notarization failed"
    echo ""
    echo "To view the log:"
    echo "xcrun notarytool log <submission-id> --key $API_KEY_PATH --key-id $API_KEY_ID --issuer $ISSUER_ID"
    exit 1
fi

echo ""
echo "✓ Notarization successful!"
echo ""

# Staple the ticket
echo "[3/4] Stapling notarization ticket..."
xcrun stapler staple "$DMG_FILE"

if [ $? -eq 0 ]; then
    echo "✓ Ticket stapled"
else
    echo "WARNING: Failed to staple (DMG may still work)"
fi

echo ""

# Verify
echo "[4/4] Verifying..."
spctl -a -vv -t install "$DMG_FILE"

echo ""
echo "========================================"
echo "Notarization Complete!"
echo "========================================"
echo ""
echo "DMG is ready for distribution"
echo "File: $DMG_FILE"
echo ""
echo "Next: Upload to GitHub release"
echo ""
