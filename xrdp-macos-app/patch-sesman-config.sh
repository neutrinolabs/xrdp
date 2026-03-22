#!/bin/bash
#
# Patch sesman.ini to use user's Library directory for runtime files
#

set -e

SESMAN_INI="$BUILT_PRODUCTS_DIR/$PRODUCT_NAME.app/Contents/Resources/etc/xrdp/sesman.ini"

if [ ! -f "$SESMAN_INI" ]; then
    echo "Warning: sesman.ini not found at $SESMAN_INI"
    exit 0
fi

# Use app's own directory for runtime files
# Note: This will be expanded to the actual installation path at build time
# For /Applications install, this becomes /Applications/xrdp.app/Contents/run
RUNTIME_DIR="/Applications/xrdp.app/Contents/run"

echo "Patching sesman.ini to use: $RUNTIME_DIR"

# Create the runtime directory in the built app
mkdir -p "$BUILT_PRODUCTS_DIR/$PRODUCT_NAME.app/Contents/run"

# Replace all socket paths to use app directory
sed -i '' "s|ListenPort=.*|ListenPort=$RUNTIME_DIR/sesman.socket|g" "$SESMAN_INI"
sed -i '' "s|/var/run/xrdp|$RUNTIME_DIR|g" "$SESMAN_INI"
sed -i '' "s|/run/user.*|$RUNTIME_DIR/user|g" "$SESMAN_INI"

echo "✅ sesman.ini patched successfully"
