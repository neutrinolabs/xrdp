#!/bin/bash
#
# Notarize macOS Package with Apple
#
# This script submits the signed package to Apple for notarization
# and staples the notarization ticket to the package.
#

set -e

PACKAGE_FILE="${1}"
BUNDLE_ID="com.xrdp.xrdp"
APPLE_ID="${APPLE_ID:-${2}}"
TEAM_ID="${TEAM_ID:-${3}}"

# Check for App Store Connect API key (optional - can also use Apple ID)
# Set these environment variables or pass as arguments:
# - API_KEY_PATH: Path to .p8 file (e.g., ~/Downloads/AuthKey_XXXXXXXXXX.p8)
# - API_KEY_ID: 10-character key ID (e.g., XXXXXXXXXX)
# - ISSUER_ID: UUID from App Store Connect API keys page
API_KEY_PATH="${API_KEY_PATH:-}"
API_KEY_ID="${API_KEY_ID:-}"
ISSUER_ID="${ISSUER_ID:-}"

if [ -z "$PACKAGE_FILE" ]; then
    echo "Usage: $0 <package-file> [apple-id] [team-id]"
    echo ""
    echo "Required:"
    echo "  package-file   Path to the .pkg file to notarize"
    echo ""
    echo "Optional (use API key OR Apple ID):"
    echo "  apple-id       Your Apple ID email (requires app-specific password)"
    echo "  team-id        Your Developer Team ID"
    echo ""
    echo "Or set environment variables:"
    echo "  export API_KEY_PATH=~/Downloads/AuthKey_XXXXXXXXXX.p8"
    echo "  export API_KEY_ID=XXXXXXXXXX"
    echo "  export ISSUER_ID=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
    exit 1
fi

echo "============================================"
echo "Notarizing macOS Package"
echo "============================================"
echo ""

# Verify package exists
if [ ! -f "$PACKAGE_FILE" ]; then
    echo "ERROR: Package not found at $PACKAGE_FILE"
    exit 1
fi

# Verify package is signed
echo "Verifying package signature..."
pkgutil --check-signature "$PACKAGE_FILE" | grep -q "signed by a developer certificate"
if [ $? -ne 0 ]; then
    echo "ERROR: Package is not signed. Sign it first with build-pkg.sh"
    exit 1
fi
echo "✓ Package is signed"
echo ""

# Check for API key (preferred method)
if [ -n "$API_KEY_PATH" ] && [ -f "$API_KEY_PATH" ]; then
    if [ -z "$API_KEY_ID" ] || [ -z "$ISSUER_ID" ]; then
        echo "ERROR: API_KEY_PATH is set but API_KEY_ID or ISSUER_ID is missing"
        echo "Set all three environment variables or use Apple ID authentication"
        exit 1
    fi

    echo "Using App Store Connect API key for notarization..."
    echo ""

    # Submit for notarization using API key
    echo "[1/3] Submitting package to Apple notary service..."
    echo "This may take several minutes..."
    echo ""

    xcrun notarytool submit "$PACKAGE_FILE" \
        --key "$API_KEY_PATH" \
        --key-id "$API_KEY_ID" \
        --issuer "$ISSUER_ID" \
        --wait

    NOTARIZE_EXIT=$?

else
    if [ -z "$APPLE_ID" ] || [ -z "$TEAM_ID" ]; then
        echo "ERROR: Neither API key nor Apple ID credentials provided"
        echo ""
        echo "Option 1 - Use App Store Connect API key:"
        echo "  export API_KEY_PATH=~/Downloads/AuthKey_XXXXXXXXXX.p8"
        echo "  export API_KEY_ID=XXXXXXXXXX"
        echo "  export ISSUER_ID=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
        echo ""
        echo "Option 2 - Use Apple ID (requires app-specific password):"
        echo "  $0 <package> <apple-id> <team-id>"
        exit 1
    fi

    echo "Using Apple ID authentication (requires app-specific password)..."
    echo "Apple ID: $APPLE_ID"
    echo "Team ID: $TEAM_ID"
    echo ""
    echo "You'll need an app-specific password from: https://appleid.apple.com/account/manage"
    echo ""

    # Submit for notarization using Apple ID
    echo "[1/3] Submitting package to Apple notary service..."
    echo "This may take several minutes..."
    echo ""

    xcrun notarytool submit "$PACKAGE_FILE" \
        --apple-id "$APPLE_ID" \
        --team-id "$TEAM_ID" \
        --wait

    NOTARIZE_EXIT=$?
fi

if [ $NOTARIZE_EXIT -ne 0 ]; then
    echo ""
    echo "ERROR: Notarization failed or was rejected"
    echo ""
    echo "To view the notarization log:"
    echo "xcrun notarytool log <submission-id> --key $API_KEY_PATH --key-id $API_KEY_ID --issuer $ISSUER_ID"
    exit 1
fi

echo ""
echo "✓ Notarization successful!"
echo ""

# Staple the notarization ticket to the package
echo "[2/3] Stapling notarization ticket to package..."
xcrun stapler staple "$PACKAGE_FILE"

if [ $? -eq 0 ]; then
    echo "✓ Notarization ticket stapled"
else
    echo "WARNING: Failed to staple ticket (package may still be valid)"
fi

echo ""

# Verify notarization
echo "[3/3] Verifying notarization..."
xcrun stapler validate "$PACKAGE_FILE"

echo ""
echo "============================================"
echo "Notarization Complete!"
echo "============================================"
echo ""
echo "The package is now notarized and ready for distribution."
echo "Users can install it without any warnings."
echo ""
echo "Package: $PACKAGE_FILE"
echo ""
