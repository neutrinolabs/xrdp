#!/bin/bash
set -e

echo "========================================="
echo "Building XRDP for macOS Distribution"
echo "========================================="

# Configuration
APP_NAME="XRDP"
BUNDLE_ID="remotex.app"
DEVELOPER_ID="Developer ID Application: Neutrinos Platforms, Inc. (H4PF9B4P9G)"
INSTALLER_ID="Developer ID Installer: Neutrinos Platforms, Inc. (H4PF9B4P9G)"
TEAM_ID="H4PF9B4P9G"

# Directories
SRCROOT="/Users/cyclic/xrdp"
XCODE_PROJECT="$SRCROOT/xrdp-macos-app"
BUILD_DIR="$XCODE_PROJECT/build/Release"
APP_PATH="$BUILD_DIR/$APP_NAME.app"
DIST_DIR="$XCODE_PROJECT/dist"

mkdir -p "$BUILD_DIR"
mkdir -p "$DIST_DIR"

echo ""
echo "Step 1: Building app with Xcode..."
cd "$XCODE_PROJECT"

# Build the app
xcodebuild clean build \
  -project xrdp.xcodeproj \
  -scheme xrdp \
  -configuration Release \
  -derivedDataPath "$BUILD_DIR/DerivedData" \
  CODE_SIGN_STYLE=Manual \
  CODE_SIGN_IDENTITY="$DEVELOPER_ID" \
  DEVELOPMENT_TEAM="$TEAM_ID" \
  PRODUCT_BUNDLE_IDENTIFIER="$BUNDLE_ID"

# Copy the built app
echo ""
echo "Step 2: Copying built app..."
cp -R "$BUILD_DIR/DerivedData/Build/Products/Release/$APP_NAME.app" "$APP_PATH"

# Copy helpers and libraries
echo ""
echo "Step 3: Copying helpers and libraries..."
HELPERS_DIR="$APP_PATH/Contents/Helpers"
LIB_DIR="$APP_PATH/Contents/Resources/lib/xrdp"

mkdir -p "$HELPERS_DIR"
mkdir -p "$LIB_DIR"

cd "$SRCROOT"
cp xrdp/.libs/xrdp "$HELPERS_DIR/xrdp"
cp sesman/.libs/xrdp-sesman "$HELPERS_DIR/xrdp-sesman"
cp sesman/chansrv/.libs/xrdp-chansrv "$HELPERS_DIR/xrdp-chansrv"

for libdir in common libipm libxrdp sesman/libsesman xup vnc mc third_party/tomlc99; do
    if [ -d "$libdir/.libs" ]; then
        cp "$libdir/.libs/"*.dylib "$LIB_DIR/" 2>/dev/null || true
    fi
done

# Copy config files
echo ""
echo "Step 4: Copying and patching config files..."
CONFIG_DIR="$APP_PATH/Contents/Resources/etc/xrdp"
mkdir -p "$CONFIG_DIR"

cp xrdp/*.ini "$CONFIG_DIR/" 2>/dev/null || true
cp sesman/*.ini "$CONFIG_DIR/" 2>/dev/null || true

# Create dummy rsakeys.ini
if [ ! -f "$CONFIG_DIR/rsakeys.ini" ]; then
    cat > "$CONFIG_DIR/rsakeys.ini" <<EOF
[keys]
# Dummy RSA keys file - not used when security_layer=tls
# XRDP is configured to use TLS with NeutrinoTLS/NeutrinoCrypto only
EOF
fi

# Run patching scripts
cd "$XCODE_PROJECT"
BUILT_PRODUCTS_DIR="$BUILD_DIR" PRODUCT_NAME="$APP_NAME" ./fix-library-paths.sh
BUILT_PRODUCTS_DIR="$BUILD_DIR" PRODUCT_NAME="$APP_NAME" ./patch-configs.sh

# Sign everything properly
echo ""
echo "Step 5: Signing app bundle with Developer ID..."

# Sign libraries
for lib in "$LIB_DIR"/*.dylib; do
    echo "Signing $(basename $lib)..."
    codesign --force --sign "$DEVELOPER_ID" --timestamp --options runtime "$lib" || true
done

# Sign helpers
for helper in "$HELPERS_DIR"/*; do
    echo "Signing $(basename $helper)..."
    codesign --force --sign "$DEVELOPER_ID" --timestamp --options runtime "$helper" || true
done

# Sign the app bundle
echo "Signing app bundle..."
codesign --force --deep --sign "$DEVELOPER_ID" --timestamp --options runtime --entitlements "$XCODE_PROJECT/xrdp.entitlements" "$APP_PATH"

# Verify signature
echo ""
echo "Step 6: Verifying signature..."
codesign --verify --deep --strict --verbose=2 "$APP_PATH"
spctl --assess --type execute --verbose=4 "$APP_PATH"

echo ""
echo "✅ App built and signed successfully at: $APP_PATH"
echo ""
echo "Next steps:"
echo "1. Create installer package: ./create-pkg.sh"
echo "2. Notarize: ./notarize.sh"
echo "3. Create DMG: ./create-dmg.sh"
