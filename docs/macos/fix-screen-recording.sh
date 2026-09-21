#!/bin/bash
#
# Fix Screen Recording Permission for macOS Screen Sharing
#
# IMPORTANT: This script must be run AFTER disabling SIP.
#
# To disable SIP:
#   Intel Mac: Reboot, hold Cmd+R, open Terminal, run: csrutil disable
#   Apple Silicon: Shut down, hold power button, Options > Terminal, run: csrutil disable
#
# After running this script, re-enable SIP for security:
#   Reboot to Recovery Mode and run: csrutil enable
#

set -e

TCC_DB="/Library/Application Support/com.apple.TCC/TCC.db"

echo "============================================"
echo "Screen Recording Permission Fix for screensharingd"
echo "============================================"
echo

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "ERROR: This script must be run with sudo"
    echo "Usage: sudo $0"
    exit 1
fi

# Check SIP status
SIP_STATUS=$(csrutil status 2>&1)
echo "SIP Status: $SIP_STATUS"
echo

if echo "$SIP_STATUS" | grep -q "enabled"; then
    echo "============================================"
    echo "ERROR: SIP is still enabled!"
    echo "============================================"
    echo
    echo "You must disable SIP first:"
    echo
    echo "For Intel Mac:"
    echo "  1. Shut down your Mac"
    echo "  2. Turn on and immediately hold Cmd + R"
    echo "  3. Open Utilities > Terminal"
    echo "  4. Run: csrutil disable"
    echo "  5. Restart and run this script again"
    echo
    echo "For Apple Silicon (M1/M2/M3):"
    echo "  1. Shut down your Mac completely"
    echo "  2. Press and hold power button until 'Loading startup options'"
    echo "  3. Click Options > Continue"
    echo "  4. Select your user, enter password"
    echo "  5. Open Utilities > Terminal"
    echo "  6. Run: csrutil disable"
    echo "  7. Restart and run this script again"
    echo
    exit 1
fi

echo "SIP is disabled. Proceeding with TCC database modification..."
echo

# Check if TCC database exists
if [ ! -f "$TCC_DB" ]; then
    echo "ERROR: TCC database not found at: $TCC_DB"
    exit 1
fi

# Backup TCC database
BACKUP="$TCC_DB.backup.$(date +%Y%m%d_%H%M%S)"
echo "Creating backup: $BACKUP"
cp "$TCC_DB" "$BACKUP"

# Get current timestamp
TIMESTAMP=$(date +%s)

# Function to add TCC entry with csreq
add_tcc_entry() {
    local SERVICE="$1"
    local CLIENT="$2"
    local CLIENT_TYPE="$3"
    local CSREQ_HEX="$4"

    echo "  Adding: $CLIENT -> $SERVICE"

    if [ -n "$CSREQ_HEX" ]; then
        sqlite3 "$TCC_DB" "INSERT OR REPLACE INTO access (service, client, client_type, auth_value, auth_reason, auth_version, csreq, indirect_object_identifier, indirect_object_identifier_type, flags, last_modified) VALUES ('$SERVICE', '$CLIENT', $CLIENT_TYPE, 2, 4, 1, X'$CSREQ_HEX', 'UNUSED', 0, 0, $TIMESTAMP);"
    else
        sqlite3 "$TCC_DB" "INSERT OR REPLACE INTO access (service, client, client_type, auth_value, auth_reason, auth_version, indirect_object_identifier, indirect_object_identifier_type, flags, last_modified) VALUES ('$SERVICE', '$CLIENT', $CLIENT_TYPE, 2, 4, 1, 'UNUSED', 0, 0, $TIMESTAMP);"
    fi
}

# Helper to add both Screen Capture and Accessibility permissions
add_screen_and_input() {
    local CLIENT="$1"
    local CLIENT_TYPE="$2"
    local CSREQ_HEX="$3"

    add_tcc_entry "kTCCServiceScreenCapture" "$CLIENT" "$CLIENT_TYPE" "$CSREQ_HEX"
    add_tcc_entry "kTCCServiceAccessibility" "$CLIENT" "$CLIENT_TYPE" "$CSREQ_HEX"
    add_tcc_entry "kTCCServicePostEvent" "$CLIENT" "$CLIENT_TYPE" "$CSREQ_HEX"
    add_tcc_entry "kTCCServiceListenEvent" "$CLIENT" "$CLIENT_TYPE" "$CSREQ_HEX"
}

# Function to generate csreq hex from a binary path
generate_csreq() {
    local BINARY="$1"
    local TMPFILE="/tmp/csreq_$$"

    if [ -f "$BINARY" ]; then
        # Get the designated requirement
        local REQ=$(codesign -dr - "$BINARY" 2>&1 | grep "designated =>" | sed 's/designated => //')
        if [ -n "$REQ" ]; then
            echo "$REQ" | csreq -r- -b "$TMPFILE" 2>/dev/null
            if [ -f "$TMPFILE" ]; then
                xxd -p "$TMPFILE" | tr -d '\n'
                rm -f "$TMPFILE"
                return
            fi
        fi
    fi
    echo ""
}

echo "Adding Screen Recording + Accessibility permissions for all Screen Sharing components..."
echo

# Add with proper csreq where possible
echo "Generating code signature requirements..."

# screensharingd
CSREQ=$(generate_csreq "/System/Library/CoreServices/RemoteManagement/screensharingd.bundle/Contents/MacOS/screensharingd")
add_screen_and_input "com.apple.screensharing.daemon" 0 "$CSREQ"

# ARDAgent
CSREQ=$(generate_csreq "/System/Library/CoreServices/RemoteManagement/ARDAgent.app/Contents/MacOS/ARDAgent")
add_screen_and_input "com.apple.RemoteDesktop.agent" 0 "$CSREQ"

# ScreenSharingSubscriber
CSREQ=$(generate_csreq "/System/Library/PrivateFrameworks/RemoteManagement.framework/XPCServices/ScreenSharingSubscriber.xpc/Contents/MacOS/ScreenSharingSubscriber")
add_screen_and_input "com.apple.remotemanagement.ScreenSharingSubscriber" 0 "$CSREQ"

# Also add path-based entries
add_screen_and_input "/System/Library/CoreServices/RemoteManagement/screensharingd.bundle/Contents/MacOS/screensharingd" 1 ""
add_screen_and_input "/System/Library/CoreServices/RemoteManagement/ARDAgent.app/Contents/MacOS/ARDAgent" 1 ""
add_screen_and_input "/System/Library/PrivateFrameworks/RemoteManagement.framework/XPCServices/ScreenSharingSubscriber.xpc/Contents/MacOS/ScreenSharingSubscriber" 1 ""
add_screen_and_input "/usr/libexec/screensharingd" 1 ""

# Generic bundle IDs
add_screen_and_input "com.apple.screensharing" 0 ""
add_screen_and_input "com.apple.screensharing.agent" 0 ""
add_screen_and_input "com.apple.RemoteDesktop" 0 ""
add_screen_and_input "com.apple.RemoteManagement" 0 ""

# xrdp components
add_screen_and_input "/usr/local/sbin/xrdp" 1 ""
add_screen_and_input "/usr/local/sbin/xrdp-sesman" 1 ""
add_screen_and_input "/usr/local/sbin/xrdp-chansrv" 1 ""
add_screen_and_input "/usr/local/lib/xrdp/libvnc.0.dylib" 1 ""

echo
echo "Done!"
echo

# Verify the entries
echo "Verifying Screen Capture entries:"
sqlite3 "$TCC_DB" "SELECT client, auth_value FROM access WHERE service='kTCCServiceScreenCapture';" | head -10
echo
echo "Verifying Accessibility entries:"
sqlite3 "$TCC_DB" "SELECT client, auth_value FROM access WHERE service='kTCCServiceAccessibility';" | head -10
echo

echo "============================================"
echo "SUCCESS! Screen Recording permissions granted."
echo "============================================"
echo
echo "Next steps:"
echo "  1. Restart the Screen Sharing service:"
echo "     sudo launchctl unload /System/Library/LaunchDaemons/com.apple.screensharing.plist"
echo "     sudo launchctl load /System/Library/LaunchDaemons/com.apple.screensharing.plist"
echo
echo "  2. Test with: python3 ~/xrdp-deps/test_vnc_pixels.py"
echo
echo "  3. IMPORTANT: Re-enable SIP for security!"
echo "     Reboot to Recovery Mode and run: csrutil enable"
echo
