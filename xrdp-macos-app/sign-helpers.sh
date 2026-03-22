#!/bin/bash

HELPERS_DIR="$BUILT_PRODUCTS_DIR/$PRODUCT_NAME.app/Contents/Helpers"
LIB_DIR="$BUILT_PRODUCTS_DIR/$PRODUCT_NAME.app/Contents/Resources/lib/xrdp"

echo "Signing libraries..."

# Sign all dylibs first (no entitlements for libraries)
for lib in "$LIB_DIR"/*.dylib; do
    if [ -f "$lib" ]; then
        echo "Signing $(basename "$lib")..."
        codesign --force --sign "$EXPANDED_CODE_SIGN_IDENTITY" \
            --options runtime \
            "$lib" || echo "Warning: Failed to sign $(basename "$lib")"
    fi
done

echo "Signing helper binaries..."

# Sign helper binaries with entitlements
for binary in "$HELPERS_DIR"/*; do
    if [ -f "$binary" ] && [ -x "$binary" ]; then
        echo "Signing $(basename "$binary")..."
        codesign --force --sign "$EXPANDED_CODE_SIGN_IDENTITY" \
            --options runtime \
            --entitlements "$CODE_SIGN_ENTITLEMENTS" \
            "$binary" || echo "Warning: Failed to sign $(basename "$binary")"
    fi
done

echo "Libraries and helper binaries signed"
