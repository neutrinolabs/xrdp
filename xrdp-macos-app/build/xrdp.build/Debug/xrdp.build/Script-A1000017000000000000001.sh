#!/bin/bash
# Copy configuration files
CONFIG_DIR="$BUILT_PRODUCTS_DIR/$PRODUCT_NAME.app/Contents/Resources/etc"

mkdir -p "$CONFIG_DIR"

# Copy config from existing installation
if [ -d "/Applications/xrdp.app/Contents/Resources/etc" ]; then
    echo "Copying config from existing installation..."
    cp -R /Applications/xrdp.app/Contents/Resources/etc/* "$CONFIG_DIR/"
fi

echo "Configuration copied"

