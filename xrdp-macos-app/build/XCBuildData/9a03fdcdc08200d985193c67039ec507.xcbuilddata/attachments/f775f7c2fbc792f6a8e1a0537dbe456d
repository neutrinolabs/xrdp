#!/bin/bash
# Copy xrdp binaries and libraries
HELPERS_DIR="$BUILT_PRODUCTS_DIR/$PRODUCT_NAME.app/Contents/Helpers"
LIB_DIR="$BUILT_PRODUCTS_DIR/$PRODUCT_NAME.app/Contents/Resources/lib/xrdp"

mkdir -p "$HELPERS_DIR"
mkdir -p "$LIB_DIR"

# Copy newly built binaries
if [ -f "$SRCROOT/../xrdp/.libs/xrdp" ]; then
    echo "Copying newly built xrdp..."
    cp "$SRCROOT/../xrdp/.libs/xrdp" "$HELPERS_DIR/xrdp"
fi

if [ -f "$SRCROOT/../sesman/.libs/xrdp-sesman" ]; then
    echo "Copying newly built xrdp-sesman..."
    cp "$SRCROOT/../sesman/.libs/xrdp-sesman" "$HELPERS_DIR/xrdp-sesman"
fi

if [ -f "$SRCROOT/../sesman/chansrv/.libs/xrdp-chansrv" ]; then
    echo "Copying newly built xrdp-chansrv..."
    cp "$SRCROOT/../sesman/chansrv/.libs/xrdp-chansrv" "$HELPERS_DIR/xrdp-chansrv"
fi

# Copy libraries from build
echo "Copying libraries..."
for libdir in common libipm libxrdp sesman/libsesman xup vnc mc third_party/tomlc99; do
    if [ -d "$SRCROOT/../$libdir/.libs" ]; then
        cp "$SRCROOT/../$libdir/.libs/"*.dylib "$LIB_DIR/" 2>/dev/null || true
    fi
done

# Also copy libmacos.dylib if it exists
if [ -f "/usr/local/lib/xrdp/libmacos.dylib" ]; then
    cp "/usr/local/lib/xrdp/libmacos.dylib" "$LIB_DIR/"
fi

# Copy OpenSSL libraries
if [ -f "/usr/local/lib/xrdp/libssl.3.dylib" ]; then
    cp "/usr/local/lib/xrdp/libssl."*.dylib "$LIB_DIR/" 2>/dev/null || true
    cp "/usr/local/lib/xrdp/libcrypto."*.dylib "$LIB_DIR/" 2>/dev/null || true
fi

echo "Helpers and libraries copied"

