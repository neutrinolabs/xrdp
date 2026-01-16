#!/bin/bash
#
# Fix library install_name paths to use @executable_path
#

set -e

APP_DIR="$BUILT_PRODUCTS_DIR/$PRODUCT_NAME.app"
LIB_DIR="$APP_DIR/Contents/Resources/lib/xrdp"

echo "Fixing library install_name paths..."

cd "$LIB_DIR"

# Fix OpenSSL library install_names
if [ -f "libssl.3.dylib" ]; then
    install_name_tool -id "@executable_path/../Resources/lib/xrdp/libssl.3.dylib" libssl.3.dylib
    install_name_tool -change "/usr/local/lib/xrdp/libcrypto.3.dylib" "@executable_path/../Resources/lib/xrdp/libcrypto.3.dylib" libssl.3.dylib
    echo "✅ Fixed libssl.3.dylib"
fi

if [ -f "libcrypto.3.dylib" ]; then
    install_name_tool -id "@executable_path/../Resources/lib/xrdp/libcrypto.3.dylib" libcrypto.3.dylib
    echo "✅ Fixed libcrypto.3.dylib"
fi

# Fix any libraries that reference /usr/local/lib/xrdp
for lib in *.dylib; do
    if [ -f "$lib" ]; then
        # Check if library references /usr/local/lib/xrdp
        if otool -L "$lib" | grep -q "/usr/local/lib/xrdp"; then
            echo "Fixing references in $lib"
            otool -L "$lib" | grep "/usr/local/lib/xrdp" | awk '{print $1}' | while read dep; do
                libname=$(basename "$dep")
                install_name_tool -change "$dep" "@executable_path/../Resources/lib/xrdp/$libname" "$lib" 2>/dev/null || true
            done
        fi
    fi
done

echo "✅ All library paths fixed"
