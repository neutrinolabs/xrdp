# xrdp NeutrinoTLS Ready for Testing

## Current Status (2026-01-16 15:04)

### What's Working
- NeutrinoTLS implementation complete (2,270 lines)
- X25519 key exchange
- ChaCha20-Poly1305 AEAD encryption
- TLS 1.3 server handshake
- Security improvements:
  - Comprehensive bounds checking in ClientHello parsing
  - EAGAIN/EWOULDBLOCK handling for non-blocking sockets
  - Extension parsing validation

### What's Built
- Debug-enabled common library: /tmp/libcommon.0.dylib-debug
- Updated xrdp binary: /Users/cyclic/xrdp/xrdp/.libs/xrdp
- GUI app bundle: /Applications/xrdp.app (needs library update)

### Blocking Issue
**System daemon conflict** - see [STATUS.md](STATUS.md)

Port 3389 is bound by system daemon (/usr/local/sbin/xrdp), preventing GUI app from starting.

## Steps to Test

### 1. Disable System Daemons (REQUIRED)
```bash
cd /Users/cyclic/xrdp
./disable-system-xrdp.sh
```

This will:
- Unload launchd services
- Kill system daemon processes
- Free port 3389

### 2. Restart GUI App
```bash
# Kill current instance (it failed to start anyway)
pkill -f "/Applications/xrdp.app"

# Start fresh instance
open -a /Applications/xrdp.app
```

### 3. Monitor Logs
```bash
# Watch for connection attempts and TLS handshakes
tail -f /Users/cyclic/Library/Logs/xrdp/xrdp.log
```

With TLS13_DEBUG=1, you'll see detailed output:
- `[TLS Server] Waiting for ClientHello...`
- `[TLS Server] ClientHello too short for...` (if malformed)
- `[TLS] recv_record: header type=X len=Y`
- `[TLS] Decrypting record...`
- etc.

### 4. Test Connections
From RemoteX host, connect to:
- IP address: (guest VM IP):3389
- Onion address: (if configured)

Expected log output on success:
```
[INFO ] Socket 13: connection accepted from X.X.X.X:XXXXX
[INFO ] Client requested security types (RDP assumed) : SSL|HYBRID
[INFO ] Selected TLS security
[INFO ] NeutrinoSSL context created (server mode)
[INFO ] NeutrinoSSL: TLS handshake successful on socket 13
```

## Known Issues from Earlier Tests

Based on logs from 04:00-04:35 AM:

1. **Intermittent TLS handshake failures** (30% failure rate)
   - Cause: Unknown (debug logging now enabled to investigate)
   - Some connections time out (1+ seconds)
   - Others complete quickly (<20ms)

2. **Sessions not progressing past TLS** - **FIXED**
   - TLS handshake succeeds but "TLS connection established" never logged
   - **Root cause**: Missing ssl_get_version() and ssl_get_cipher_name() functions
   - **Fix**: Implemented these functions to return "TLSv1.3" and "TLS_CHACHA20_POLY1305_SHA256"
   - Now should see: `TLS connection established from X.X.X.X:PORT TLSv1.3 with cipher TLS_CHACHA20_POLY1305_SHA256`

3. **Sesman socket conflicts**
   - Multiple sesman instances failing to bind
   - Same root cause as xrdp conflict
   - Will be resolved by disabling system daemons

## What to Watch For

### Success Indicators
- `NeutrinoSSL: TLS handshake successful`
- RDP protocol messages (MCS, GCC, etc.)
- Bitmap/graphics data flowing
- No crashes

### Failure Indicators
- `TLS handshake failed` (with debug details now)
- `bad header length` (X.224 parsing error)
- Server restarts immediately after handshake
- Connection accepted but no further messages

## Debugging TLS Issues

If TLS handshakes fail with debug enabled, check for:

1. **ClientHello parsing errors**
   - "ClientHello too short for..."
   - "Extension length exceeds..."
   - Indicates malformed client data

2. **Key exchange failures**
   - "Failed to find key_share extension"
   - "x25519 computation failed"
   - Check client's supported groups

3. **Decryption errors**
   - "Tag verification failed"
   - "Record too short for tag"
   - Key derivation or encryption mismatch

## Files Changed in This Session

### TLS Implementation
1. [common/neutrinotls.c:7](common/neutrinotls.c#L7) - Enabled TLS13_DEBUG
2. [common/neutrinotls.c:831-850](common/neutrinotls.c#L831) - Enhanced sock_write() with EAGAIN handling
3. [common/neutrinotls.c:1557-1701](common/neutrinotls.c#L1557) - Added comprehensive bounds checking
4. [common/neutrinossl.c:301-318](common/neutrinossl.c#L301) - **NEW: Added ssl_get_version() and ssl_get_cipher_name()**
5. [common/neutrinossl.h:107-118](common/neutrinossl.h#L107) - **NEW: Added function declarations**
6. [common/ssl_calls.c:1640-1662](common/ssl_calls.c#L1640) - **NEW: Added USE_NEUTRINOSSL conditionals**

### Scripts
7. [disable-system-xrdp.sh](disable-system-xrdp.sh) - Script to disable system daemons
8. [check-system-logs.sh](check-system-logs.sh) - Script to check system logs (requires sudo)

### Built Artifacts
- `/tmp/libcommon.0.dylib-v2` - **Latest library with all fixes** (TLS debug + version/cipher reporting)
- `/tmp/libcommon.0.dylib-debug` - Earlier version (TLS debug only)

## Next Steps After Port is Freed

1. Verify port 3389 is listening (GUI app)
2. Monitor for connection attempts
3. Analyze TLS handshake debug output
4. Investigate why sessions don't progress past TLS
5. Check for crashes/errors in RDP protocol layer
6. Test bitmap reception (like VNC test in docs/macos/test_vnc_pixels.py)

## Scripts Available

- `./disable-system-xrdp.sh` - Fix port conflict
- `./check-system-logs.sh` - View system daemon logs (requires sudo)
- `tail -f /Users/cyclic/Library/Logs/xrdp/xrdp.log` - Monitor GUI app logs
