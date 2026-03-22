#!/bin/bash
#
# Fix library install_name paths to use @executable_path
#

set -e

APP_DIR="$BUILT_PRODUCTS_DIR/$PRODUCT_NAME.app"
LIB_DIR="$APP_DIR/Contents/Resources/lib/xrdp"

echo "Fixing library install_name paths..."

# Skip if no libraries were copied
if [ ! -d "$LIB_DIR" ] || [ -z "$(ls -A "$LIB_DIR" 2>/dev/null)" ]; then
    echo "No libraries to fix (directory empty or doesn't exist)"
    exit 0
fi

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

# Fix any libraries that reference /usr/local/lib/xrdp or @rpath
for lib in *.dylib; do
    if [ -f "$lib" ]; then
        # Fix library ID if it uses @rpath
        if otool -L "$lib" | head -2 | tail -1 | grep -q "@rpath"; then
            install_name_tool -id "@executable_path/../Resources/lib/xrdp/$lib" "$lib" 2>/dev/null || true
        fi

        # Check if library references /usr/local/lib/xrdp
        if otool -L "$lib" | grep -q "/usr/local/lib/xrdp"; then
            echo "Fixing /usr/local references in $lib"
            otool -L "$lib" | grep "/usr/local/lib/xrdp" | awk '{print $1}' | while read dep; do
                libname=$(basename "$dep")
                install_name_tool -change "$dep" "@executable_path/../Resources/lib/xrdp/$libname" "$lib" 2>/dev/null || true
            done
        fi

        # Check if library references @rpath
        if otool -L "$lib" | grep -q "@rpath"; then
            echo "Fixing @rpath references in $lib"
            otool -L "$lib" | grep "@rpath" | awk '{print $1}' | while read dep; do
                libname=$(basename "$dep")
                install_name_tool -change "$dep" "@executable_path/../Resources/lib/xrdp/$libname" "$lib" 2>/dev/null || true
            done
        fi
    fi
done

echo "✅ All library paths fixed"

# Fix helper binaries to use @executable_path
HELPERS_DIR="$APP_DIR/Contents/Helpers"
echo "Fixing helper binary paths..."

for binary in "$HELPERS_DIR"/*; do
    if [ -f "$binary" ] && [ -x "$binary" ]; then
        echo "Fixing $(basename "$binary")..."
        # Change /Applications paths to @executable_path (handle both xrdp.app and XRDP.app)
        otool -L "$binary" | grep "/Applications/.*\.app/Contents/Resources/lib/xrdp" | awk '{print $1}' | while read dep; do
            libname=$(basename "$dep")
            install_name_tool -change "$dep" "@executable_path/../Resources/lib/xrdp/$libname" "$binary" 2>/dev/null || true
        done
    fi
done

echo "✅ Helper binaries fixed"
