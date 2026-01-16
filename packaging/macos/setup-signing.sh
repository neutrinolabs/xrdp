#!/bin/bash
#
# Setup Code Signing for xrdp macOS Package
# This script creates a Developer ID certificate via App Store Connect API
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
API_KEY_PATH="$HOME/Downloads/AuthKey_N5Q2ZKTJB5.p8"
API_KEY_ID="N5Q2ZKTJB5"
ISSUER_ID="e73e7c0f-7ade-4a3f-bf61-f9c176b84abc"

echo "============================================"
echo "  xrdp Code Signing Setup"
echo "============================================"
echo ""

# Check if certificate already exists
EXISTING_CERT=$(security find-identity -v -p codesigning | grep "Developer ID Application" | head -1 || true)
if [ -n "$EXISTING_CERT" ]; then
    echo "✓ Developer ID Application certificate already installed:"
    echo "  $EXISTING_CERT"
    echo ""
    echo "Certificate is ready for signing."
    exit 0
fi

echo "No Developer ID certificate found. Creating one..."
echo ""

# Check for API key
if [ ! -f "$API_KEY_PATH" ]; then
    echo "ERROR: API key not found at $API_KEY_PATH"
    exit 1
fi

# Generate CSR
echo "[1/4] Generating Certificate Signing Request..."
CSR_PATH="/tmp/xrdp-dev-id.csr"
KEY_PATH="/tmp/xrdp-dev-id.key"

openssl req -new -newkey rsa:2048 -nodes \
    -keyout "$KEY_PATH" \
    -out "$CSR_PATH" \
    -subj "/emailAddress=admin@remotex.app/CN=Developer ID Application/O=RemoteX" \
    2>/dev/null

echo "  CSR created: $CSR_PATH"

# Generate JWT token for API authentication
echo "[2/4] Generating API authentication token..."

# Install PyJWT if needed
pip3 install -q PyJWT cryptography 2>/dev/null || true

# Create JWT using proper ES256 signing
JWT=$(python3 <<PYTHON_EOF
import jwt
import time

with open("$API_KEY_PATH", "r") as f:
    private_key = f.read()

payload = {
    "iss": "$ISSUER_ID",
    "exp": int(time.time()) + 1200,
    "aud": "appstoreconnect-v1"
}

token = jwt.encode(
    payload,
    private_key,
    algorithm="ES256",
    headers={"kid": "$API_KEY_ID"}
)

print(token)
PYTHON_EOF
)

# Create certificate via API
echo "[3/4] Creating Developer ID certificate via API..."

CSR_CONTENT=$(cat "$CSR_PATH" | tr -d '\n')

API_RESPONSE=$(curl -s -X POST \
    -H "Authorization: Bearer $JWT" \
    -H "Content-Type: application/json" \
    -d '{
        "data": {
            "type": "certificates",
            "attributes": {
                "certificateType": "DEVELOPER_ID_APPLICATION",
                "csrContent": "'"$CSR_CONTENT"'"
            }
        }
    }' \
    "https://api.appstoreconnect.apple.com/v1/certificates")

# Check for errors
if echo "$API_RESPONSE" | grep -q "errors"; then
    echo "ERROR: Failed to create certificate"
    echo "$API_RESPONSE" | python3 -m json.tool 2>/dev/null || echo "$API_RESPONSE"
    exit 1
fi

# Extract certificate
CERT_CONTENT=$(echo "$API_RESPONSE" | python3 -c "import sys, json; print(json.load(sys.stdin)['data']['attributes']['certificateContent'])" 2>/dev/null)

if [ -z "$CERT_CONTENT" ]; then
    echo "ERROR: Could not extract certificate from API response"
    exit 1
fi

CERT_PATH="/tmp/xrdp-dev-id.cer"
echo "$CERT_CONTENT" | base64 -d > "$CERT_PATH"

echo "  Certificate created successfully"

# Import certificate and private key into Keychain
echo "[4/4] Installing certificate in Keychain..."

# Convert to PKCS12 format
P12_PATH="/tmp/xrdp-dev-id.p12"
openssl pkcs12 -export \
    -inkey "$KEY_PATH" \
    -in "$CERT_PATH" \
    -out "$P12_PATH" \
    -passout pass:temporary 2>/dev/null

# Import into login keychain
security import "$P12_PATH" \
    -k ~/Library/Keychains/login.keychain-db \
    -P temporary \
    -T /usr/bin/codesign \
    -T /usr/bin/productsign

# Set key partition list (allows codesign to access the key)
security set-key-partition-list \
    -S apple-tool:,apple: \
    -k "$(security unlock-keychain ~/Library/Keychains/login.keychain-db 2>&1 | grep -o 'password:.*' | cut -d: -f2 || echo '')" \
    ~/Library/Keychains/login.keychain-db 2>/dev/null || true

echo "  Certificate installed"

# Clean up
rm -f "$CSR_PATH" "$KEY_PATH" "$CERT_PATH" "$P12_PATH"

echo ""
echo "============================================"
echo "  Setup Complete!"
echo "============================================"
echo ""
echo "Developer ID certificate is now installed and ready for signing."
echo ""

# Show installed certificate
security find-identity -v -p codesigning | grep "Developer ID Application"

echo ""
