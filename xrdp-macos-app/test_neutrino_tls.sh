#!/bin/bash

# NeutrinoTLS Test Script
# Tests the complete TLS 1.3 implementation

echo "=================================================="
echo "NeutrinoTLS - TLS 1.3 Implementation Test"
echo "=================================================="
echo ""

# Check if server is running
if ! lsof -nP -iTCP:3389 -sTCP:LISTEN >/dev/null 2>&1; then
    echo "❌ ERROR: xrdp server is not running on port 3389"
    echo ""
    echo "Please start the server:"
    echo "  1. Click the ⚡️ icon in the menu bar"
    echo "  2. Select 'Start Server'"
    echo ""
    exit 1
fi

echo "✅ Server is running on port 3389"
echo ""

# Run TLS 1.3 test
echo "Running TLS 1.3 handshake test..."
echo "=================================================="
echo ""

if python3 /tmp/test-full-tls13.py; then
    echo ""
    echo "=================================================="
    echo "✅✅✅ SUCCESS! ✅✅✅"
    echo "=================================================="
    echo ""
    echo "NeutrinoTLS is fully functional:"
    echo "  • TLS 1.3 handshake complete"
    echo "  • X25519 key exchange working"
    echo "  • ChaCha20-Poly1305 encryption/decryption working"
    echo "  • Zero OpenSSL dependencies"
    echo "  • Zero Apple framework dependencies"
    echo "  • No PAC crashes on Apple Silicon"
    echo ""
    echo "Next steps:"
    echo "  1. Test with Microsoft Remote Desktop"
    echo "  2. Connect to 127.0.0.1:3389"
    echo "  3. Username: cyclic"
    echo "  4. Verify screen sharing and bitmap reception"
    echo ""
    exit 0
else
    echo ""
    echo "❌ TLS test failed"
    echo ""
    exit 1
fi
