# Quick Reference - xrdp NeutrinoTLS

## Current Situation

**Status**: Server NOT listening on port 3389
**Reason**: System daemon conflict
**Solution**: Run `./disable-system-xrdp.sh`

## Key Files

### Logs
```bash
# GUI app logs (current)
tail -f /Users/cyclic/Library/Logs/xrdp/xrdp.log

# System daemon logs (requires sudo)
sudo tail -f /var/log/xrdp.log

# Check system logs
./check-system-logs.sh
```

### Binaries
```
/Applications/xrdp.app/Contents/MacOS/xrdp    # GUI app (currently failing to start)
/usr/local/sbin/xrdp                          # System daemon (blocking port)
```

### Libraries
```
/tmp/libcommon.0.dylib-v2                     # Latest build (ready to deploy)
/Applications/xrdp.app/Contents/Resources/lib/ # App bundle libraries
```

## Quick Commands

### Check Status
```bash
# Is port 3389 listening?
lsof -nP -iTCP:3389 -sTCP:LISTEN

# What processes are running?
ps aux | grep -E "(xrdp|sesman)" | grep -v grep

# Recent log entries?
tail -20 /Users/cyclic/Library/Logs/xrdp/xrdp.log
```

### Fix Port Conflict
```bash
cd /Users/cyclic/xrdp
./disable-system-xrdp.sh
```

### Restart Server
```bash
# Stop GUI app
pkill -f "/Applications/xrdp.app"

# Start GUI app
open -a /Applications/xrdp.app

# Verify it's listening
lsof -nP -iTCP:3389 -sTCP:LISTEN
```

### Monitor Connections
```bash
# Watch logs in real-time
tail -f /Users/cyclic/Library/Logs/xrdp/xrdp.log

# Watch for TLS handshakes
tail -f /Users/cyclic/Library/Logs/xrdp/xrdp.log | grep -E "(handshake|TLS|connection)"

# Check network connections
netstat -an | grep 3389
```

## Expected Log Output

### Successful Connection
```
[INFO ] Socket 13: connection accepted from 192.168.64.1:12345
[INFO ] Client requested security types (RDP assumed) : SSL|HYBRID
[INFO ] Selected TLS security
[INFO ] NeutrinoSSL context created (server mode)
[INFO ] NeutrinoSSL: TLS handshake successful on socket 13
[INFO ] TLS connection established from 192.168.64.1:12345 TLSv1.3 with cipher TLS_CHACHA20_POLY1305_SHA256
```

### Failed Handshake
```
[ERROR] NeutrinoSSL: TLS handshake failed on socket 13
[ERROR] TLS handshake failed: TLS server handshake failed
[ERROR] trans_set_tls_mode: ssl_tls_accept failed
```

### Port Conflict Error
```
[ERROR] trans_listen_address failed
[CORE ] Failed to start xrdp daemon, possibly address already in use.
```

## Troubleshooting

### Port Already in Use
```bash
# Check what's using the port
lsof -nP -iTCP:3389

# If it's the system daemon, disable it
./disable-system-xrdp.sh
```

### No Connection Attempts
```bash
# Verify server is listening
lsof -nP -iTCP:3389 -sTCP:LISTEN

# Check firewall
sudo /usr/libexec/ApplicationFirewall/socketfilterfw --getglobalstate

# Test locally
nc -z 127.0.0.1 3389
```

### TLS Handshake Failures
```bash
# Check for detailed debug output (requires TLS13_DEBUG=1)
grep "\\[TLS" /Users/cyclic/Library/Logs/xrdp/xrdp.log

# If no debug output, rebuild with debug enabled
cd /Users/cyclic/xrdp
grep "TLS13_DEBUG" common/neutrinotls.c  # Should show "1"
```

## Documentation

- [STATUS.md](STATUS.md) - Detailed status and issues
- [READY_FOR_TESTING.md](READY_FOR_TESTING.md) - Complete testing guide
- [SESSION_SUMMARY.md](SESSION_SUMMARY.md) - Summary of changes made
- [QUICK_REFERENCE.md](QUICK_REFERENCE.md) - This file
