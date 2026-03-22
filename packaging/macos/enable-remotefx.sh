#!/bin/bash
#
# Enable RemoteFX and Advanced Features for xrdp on macOS
#
# This script configures xrdp with optimal settings for:
# - RemoteFX (RFX) codec
# - H.264 video codec
# - Graphics Pipeline Extension (GFX)
# - Enhanced performance settings
#

set -e

XRDP_INI="/etc/xrdp/xrdp.ini"
BACKUP_DIR="/etc/xrdp/backups"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

echo "============================================"
echo "  xrdp RemoteFX & Advanced Features Setup"
echo "============================================"
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "ERROR: This script must be run as root"
    echo "Usage: sudo $0"
    exit 1
fi

# Check if xrdp is installed
if [ ! -f "$XRDP_INI" ]; then
    echo "ERROR: xrdp configuration not found at $XRDP_INI"
    echo "Please install xrdp first."
    exit 1
fi

# Create backup directory
mkdir -p "$BACKUP_DIR"

# Backup current configuration
echo "[1/4] Backing up current configuration..."
cp "$XRDP_INI" "$BACKUP_DIR/xrdp.ini.$TIMESTAMP"
echo "  Backup saved to: $BACKUP_DIR/xrdp.ini.$TIMESTAMP"
echo ""

# Apply RemoteFX and advanced settings
echo "[2/4] Configuring RemoteFX and advanced features..."

# Update or add settings in [Globals] section
update_global_setting() {
    local key="$1"
    local value="$2"

    if grep -q "^${key}=" "$XRDP_INI"; then
        # Setting exists, update it
        sed -i.tmp "s|^${key}=.*|${key}=${value}|g" "$XRDP_INI"
    elif grep -q "^#${key}=" "$XRDP_INI"; then
        # Setting exists but is commented, uncomment and update
        sed -i.tmp "s|^#${key}=.*|${key}=${value}|g" "$XRDP_INI"
    else
        # Setting doesn't exist, add it after [Globals]
        sed -i.tmp "/^\[Globals\]/a\\
${key}=${value}
" "$XRDP_INI"
    fi
    rm -f "${XRDP_INI}.tmp"
}

# Performance settings
update_global_setting "max_bpp" "32"
update_global_setting "use_fastpath" "both"
update_global_setting "bitmap_cache" "true"
update_global_setting "bitmap_compression" "true"
update_global_setting "bulk_compression" "true"
update_global_setting "new_cursors" "true"

# Multi-monitor and channels
update_global_setting "allow_channels" "true"
update_global_setting "allow_multimon" "true"

# Optimize frame intervals for better performance
# RemoteFX at 60 fps, H.264 at 60 fps
sed -i.tmp 's|^rfx_frame_interval=.*|rfx_frame_interval=16|g' "$XRDP_INI"
sed -i.tmp 's|^h264_frame_interval=.*|h264_frame_interval=16|g' "$XRDP_INI"
rm -f "${XRDP_INI}.tmp"

echo "  ✓ Performance settings optimized"
echo "  ✓ RemoteFX frame interval: 16ms (~60 fps)"
echo "  ✓ H.264 frame interval: 16ms (~60 fps)"
echo "  ✓ Fast-path enabled for input and output"
echo "  ✓ Multi-monitor support enabled"
echo ""

# Add GFX settings if not present
echo "[3/4] Enabling Graphics Pipeline Extension (GFX)..."

# Check if GFX settings already exist
if ! grep -q "^gfx=" "$XRDP_INI" && ! grep -q "^#gfx=" "$XRDP_INI"; then
    # Add GFX settings to [Globals] section
    sed -i.tmp "/^\[Globals\]/a\\
\\
; Graphics Pipeline Extension (RDP 8.0+)\\
; Enable modern codecs and progressive rendering\\
gfx=true\\
gfx_progressive=true\\
gfx_h264=true\\
gfx_rfx=true\\
" "$XRDP_INI"
    rm -f "${XRDP_INI}.tmp"
    echo "  ✓ GFX settings added"
else
    # Update existing GFX settings
    update_global_setting "gfx" "true"
    update_global_setting "gfx_progressive" "true"
    update_global_setting "gfx_h264" "true"
    update_global_setting "gfx_rfx" "true"
    echo "  ✓ GFX settings updated"
fi
echo ""

# Restart xrdp services
echo "[4/4] Restarting xrdp services..."
launchctl unload /Library/LaunchDaemons/com.xrdp.xrdp.plist 2>/dev/null || true
launchctl unload /Library/LaunchDaemons/com.xrdp.sesman.plist 2>/dev/null || true
sleep 2
launchctl load /Library/LaunchDaemons/com.xrdp.sesman.plist
launchctl load /Library/LaunchDaemons/com.xrdp.xrdp.plist
sleep 2

# Check if services are running
XRDP_RUNNING=$(ps aux | grep "[x]rdp" | grep -v "xrdp-" | wc -l | tr -d ' ')
SESMAN_RUNNING=$(ps aux | grep "[x]rdp-sesman" | wc -l | tr -d ' ')

if [ "$XRDP_RUNNING" -gt 0 ] && [ "$SESMAN_RUNNING" -gt 0 ]; then
    echo "  ✓ Services restarted successfully"
else
    echo "  ⚠ Warning: Services may not have started correctly"
    echo "  Check logs at /var/log/xrdp.log and /var/log/xrdp-sesman.log"
fi

echo ""
echo "============================================"
echo "  Configuration Complete!"
echo "============================================"
echo ""
echo "RemoteFX and advanced features are now enabled."
echo ""
echo "Enabled features:"
echo "  • RemoteFX (RFX) codec at 60 fps"
echo "  • H.264 video codec at 60 fps"
echo "  • Graphics Pipeline Extension (GFX)"
echo "  • Progressive rendering"
echo "  • Multi-monitor support"
echo "  • Fast-path I/O"
echo "  • Bitmap caching and compression"
echo ""
echo "Testing RemoteFX connection:"
echo "  From Windows: Use Remote Desktop Connection (mstsc.exe)"
echo "  From macOS: Use Microsoft Remote Desktop app"
echo "  From Linux: xfreerdp /v:$(hostname):3389 /u:USER /rfx /gfx:rfx"
echo ""
echo "Monitoring:"
echo "  View logs: tail -f /var/log/xrdp.log"
echo "  Check status: sudo launchctl list | grep xrdp"
echo "  View processes: ps aux | grep '[x]rdp'"
echo ""
echo "To restore previous configuration:"
echo "  sudo cp $BACKUP_DIR/xrdp.ini.$TIMESTAMP $XRDP_INI"
echo "  sudo launchctl unload /Library/LaunchDaemons/com.xrdp.xrdp.plist"
echo "  sudo launchctl load /Library/LaunchDaemons/com.xrdp.xrdp.plist"
echo ""
