#!/bin/bash
#
# xrdp Troubleshooting and Fix Script for macOS
#
# This script diagnoses and fixes common xrdp installation issues on macOS
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INSTALL_PREFIX="/usr/local"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

print_status() {
    if [ "$1" = "ok" ]; then
        echo -e "${GREEN}✅ $2${NC}"
    elif [ "$1" = "error" ]; then
        echo -e "${RED}❌ $2${NC}"
    else
        echo -e "${YELLOW}⚠️  $2${NC}"
    fi
}

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "ERROR: This script must be run with sudo"
    echo "Usage: sudo $0"
    exit 1
fi

echo "============================================"
echo "  xrdp Troubleshooting and Fix Script"
echo "============================================"
echo ""

# Step 1: Check installation
echo "[1/7] Checking xrdp installation..."
ISSUES_FOUND=0

if [ -f "$INSTALL_PREFIX/sbin/xrdp" ]; then
    print_status "ok" "xrdp binary found"
else
    print_status "error" "xrdp binary not found at $INSTALL_PREFIX/sbin/xrdp"
    ISSUES_FOUND=$((ISSUES_FOUND + 1))
fi

if [ -f "$INSTALL_PREFIX/sbin/xrdp-sesman" ]; then
    print_status "ok" "xrdp-sesman binary found"
else
    print_status "error" "xrdp-sesman binary not found"
    ISSUES_FOUND=$((ISSUES_FOUND + 1))
fi

echo ""

# Step 2: Check bundled libraries
echo "[2/7] Checking bundled libraries..."
BUNDLED_LIBS_OK=true

if [ ! -d "$INSTALL_PREFIX/lib/xrdp" ]; then
    print_status "error" "Bundled library directory not found at $INSTALL_PREFIX/lib/xrdp"
    BUNDLED_LIBS_OK=false
    ISSUES_FOUND=$((ISSUES_FOUND + 1))
else
    # Check for OpenSSL libraries
    if [ -f "$INSTALL_PREFIX/lib/xrdp/libssl.3.dylib" ] && [ -f "$INSTALL_PREFIX/lib/xrdp/libcrypto.3.dylib" ]; then
        print_status "ok" "OpenSSL libraries bundled"
    else
        print_status "error" "OpenSSL libraries missing from bundle"
        BUNDLED_LIBS_OK=false
        ISSUES_FOUND=$((ISSUES_FOUND + 1))
    fi

    # Check for NeutrinoRDP libraries
    FREERDP_COUNT=$(ls -1 "$INSTALL_PREFIX/lib/xrdp"/libfreerdp-*.dylib 2>/dev/null | wc -l | tr -d ' ')
    if [ "$FREERDP_COUNT" -ge 7 ]; then
        print_status "ok" "NeutrinoRDP libraries bundled ($FREERDP_COUNT found)"
    else
        print_status "warn" "Only $FREERDP_COUNT NeutrinoRDP libraries found (expected 7-8)"
    fi
fi

echo ""

# Step 3: Check for conflicting system libraries
echo "[3/7] Checking for conflicting system libraries..."
CONFLICTS_FOUND=0
CONFLICTS=$(ls -1 /usr/local/lib/libfreerdp-*.dylib 2>/dev/null | wc -l | tr -d ' ')

if [ "$CONFLICTS" -gt 0 ]; then
    print_status "error" "Found $CONFLICTS conflicting NeutrinoRDP libraries in /usr/local/lib"
    echo "   These libraries can interfere with bundled versions"
    ISSUES_FOUND=$((ISSUES_FOUND + 1))
    CONFLICTS_FOUND=1

    echo ""
    read -p "   Remove conflicting libraries? (y/n) " -n 1 -r
    echo ""
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "   Removing conflicting libraries..."
        rm -f /usr/local/lib/libfreerdp-*.dylib
        REMOVED=$(ls -1 /usr/local/lib/libfreerdp-*.dylib 2>/dev/null | wc -l | tr -d ' ')
        if [ "$REMOVED" -eq 0 ]; then
            print_status "ok" "All conflicting libraries removed"
            CONFLICTS_FOUND=0
            ISSUES_FOUND=$((ISSUES_FOUND - 1))
        else
            print_status "error" "$REMOVED library files remain"
        fi
    fi
else
    print_status "ok" "No conflicting system libraries found"
fi

echo ""

# Step 4: Check RSA keys
echo "[4/7] Checking RSA keys..."
if [ -f "/etc/xrdp/rsakeys.ini" ]; then
    print_status "ok" "RSA keys file exists"
else
    print_status "warn" "RSA keys file missing"
    echo "   Generating RSA keys..."
    if [ -f "$INSTALL_PREFIX/bin/xrdp-keygen" ]; then
        $INSTALL_PREFIX/bin/xrdp-keygen xrdp /etc/xrdp/rsakeys.ini
        print_status "ok" "RSA keys generated"
    else
        print_status "error" "xrdp-keygen not found, cannot generate keys"
        ISSUES_FOUND=$((ISSUES_FOUND + 1))
    fi
fi

echo ""

# Step 5: Check LaunchDaemons
echo "[5/7] Checking LaunchDaemons..."
if [ -f "/Library/LaunchDaemons/com.xrdp.xrdp.plist" ]; then
    print_status "ok" "xrdp LaunchDaemon installed"
else
    print_status "error" "xrdp LaunchDaemon missing"
    ISSUES_FOUND=$((ISSUES_FOUND + 1))
fi

if [ -f "/Library/LaunchDaemons/com.xrdp.sesman.plist" ]; then
    print_status "ok" "xrdp-sesman LaunchDaemon installed"
else
    print_status "error" "xrdp-sesman LaunchDaemon missing"
    ISSUES_FOUND=$((ISSUES_FOUND + 1))
fi

echo ""

# Step 6: Restart services
echo "[6/7] Restarting xrdp services..."
echo "   Stopping services..."
pkill -9 xrdp 2>/dev/null || true
pkill -9 xrdp-sesman 2>/dev/null || true
launchctl bootout system/com.xrdp.xrdp 2>/dev/null || true
launchctl bootout system/com.xrdp.sesman 2>/dev/null || true
sleep 2

echo "   Starting services..."
launchctl bootstrap system /Library/LaunchDaemons/com.xrdp.sesman.plist 2>/dev/null || true
sleep 1
launchctl bootstrap system /Library/LaunchDaemons/com.xrdp.xrdp.plist 2>/dev/null || true
sleep 3

print_status "ok" "Services restarted"
echo ""

# Step 7: Verify services and connectivity
echo "[7/7] Verifying services..."
echo ""

XRDP_RUNNING=0
SESMAN_RUNNING=0
PORT_LISTENING=0

if pgrep -q "^xrdp$"; then
    XRDP_PID=$(pgrep "^xrdp$")
    print_status "ok" "xrdp server is running (PID: $XRDP_PID)"
    XRDP_RUNNING=1
else
    print_status "error" "xrdp server is NOT running"
    ISSUES_FOUND=$((ISSUES_FOUND + 1))

    # Try to get error details
    echo "   Attempting to start xrdp manually for diagnostics..."
    ERROR_OUTPUT=$($INSTALL_PREFIX/sbin/xrdp --nodaemon 2>&1 &)
    MANUAL_PID=$!
    sleep 2

    if ps -p $MANUAL_PID > /dev/null 2>&1; then
        print_status "ok" "Manual start successful"
        kill $MANUAL_PID 2>/dev/null
        XRDP_RUNNING=1
        ISSUES_FOUND=$((ISSUES_FOUND - 1))
    else
        echo "   Error: $ERROR_OUTPUT"
    fi
fi

if pgrep -q "xrdp-sesman"; then
    SESMAN_PID=$(pgrep "xrdp-sesman")
    print_status "ok" "xrdp-sesman is running (PID: $SESMAN_PID)"
    SESMAN_RUNNING=1
else
    print_status "error" "xrdp-sesman is NOT running"
    ISSUES_FOUND=$((ISSUES_FOUND + 1))
fi

if lsof -i :3389 >/dev/null 2>&1; then
    print_status "ok" "Port 3389 is listening"
    PORT_LISTENING=1
else
    print_status "error" "Port 3389 is NOT listening"
    ISSUES_FOUND=$((ISSUES_FOUND + 1))
fi

echo ""
echo "============================================"
echo "  Summary"
echo "============================================"
echo ""

if [ $ISSUES_FOUND -eq 0 ] && [ $XRDP_RUNNING -eq 1 ] && [ $SESMAN_RUNNING -eq 1 ] && [ $PORT_LISTENING -eq 1 ]; then
    print_status "ok" "xrdp is fully operational!"
    echo ""
    echo "Connection Information:"
    echo "  Port: 3389"
    echo "  Protocol: RDP"
    echo ""
    echo "Next steps:"
    echo "  1. Enable Screen Sharing in System Settings"
    echo "  2. Connect via RDP client to port 3389"
    echo ""
    echo "Optional:"
    echo "  Enable RemoteFX (60fps H.264): sudo $INSTALL_PREFIX/share/xrdp/enable-remotefx.sh"
    echo ""
else
    print_status "error" "Found $ISSUES_FOUND issue(s)"
    echo ""
    echo "Troubleshooting steps:"
    echo "  1. Check logs:"
    echo "     - tail -50 /var/log/xrdp.log"
    echo "     - tail -50 /var/log/xrdp-sesman.log"
    echo "     - tail -50 /var/log/xrdp.err"
    echo ""
    echo "  2. Verify library paths:"
    echo "     - otool -L $INSTALL_PREFIX/sbin/xrdp | grep lib"
    echo ""
    echo "  3. Check for library conflicts:"
    echo "     - ls -la /usr/local/lib/libfreerdp-*"
    echo ""
    echo "  4. Re-run this script: sudo $0"
    echo ""
fi

echo "============================================"
echo ""

exit $ISSUES_FOUND
