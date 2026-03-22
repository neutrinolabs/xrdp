#!/bin/bash
SESMAN_INI="$BUILT_PRODUCTS_DIR/$PRODUCT_NAME.app/Contents/Resources/etc/xrdp/sesman.ini"
XRDP_INI="$BUILT_PRODUCTS_DIR/$PRODUCT_NAME.app/Contents/Resources/etc/xrdp/xrdp.ini"

if [ ! -f "$SESMAN_INI" ]; then
    echo "Warning: sesman.ini not found at $SESMAN_INI"
    exit 0
fi

if [ ! -f "$XRDP_INI" ]; then
    echo "Warning: xrdp.ini not found at $XRDP_INI"
    exit 0
fi

RUNTIME_DIR="/Applications/$PRODUCT_NAME.app/Contents/run"
echo "Patching xrdp configuration files..."
echo "Using runtime directory: $RUNTIME_DIR"

mkdir -p "$BUILT_PRODUCTS_DIR/$PRODUCT_NAME.app/Contents/run"

# Patch sesman.ini to use app bundle socket paths
# First fix any existing /Applications paths (both uppercase and lowercase)
sed -i '' "s|/Applications/[Xx][Rr][Dd][Pp]\.app/Contents/run|$RUNTIME_DIR|g" "$SESMAN_INI"
sed -i '' "s|/Applications/[Xx][Rr][Dd][Pp]\.app/Contents/Resources|/Applications/$PRODUCT_NAME.app/Contents/Resources|g" "$SESMAN_INI"
# Then apply the standard patches
sed -i '' "s|ListenPort=.*|ListenPort=$RUNTIME_DIR/sesman.socket|g" "$SESMAN_INI"
sed -i '' "s|/var/run/xrdp|$RUNTIME_DIR|g" "$SESMAN_INI"
sed -i '' "s|/run/user.*|$RUNTIME_DIR/user|g" "$SESMAN_INI"

echo "✅ sesman.ini patched successfully"

# Patch xrdp.ini to disable fork mode on macOS (use threading instead)
sed -i '' "s|^fork=true|fork=false|g" "$XRDP_INI"

echo "✅ xrdp.ini patched to use threading instead of fork"
