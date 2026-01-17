# xrdp Server Status - 2026-01-16 15:02

## Current Issues

### CRITICAL: Port 3389 Binding Conflict

**Status**: BLOCKING - GUI app with NeutrinoTLS cannot accept connections

**Problem**:
- Two xrdp instances trying to bind port 3389:
  - System daemon (PID 360, root, /usr/local/sbin/xrdp) - currently listening
  - GUI app (PID 9485, cyclic, /Applications/xrdp.app/Contents/MacOS/xrdp) - FAILING to start
- System daemon does NOT have NeutrinoTLS (still using old OpenSSL)
- GUI app has NeutrinoTLS but cannot bind port
- Connection attempts go to system daemon, not GUI app

**Logging Locations**:
- System daemon logs: /var/log/xrdp.log (requires sudo to read, last updated 14:58)
- GUI app logs: /Users/cyclic/Library/Logs/xrdp/xrdp.log (last updated 14:44 when it failed to start)

**Evidence**:
```
GUI app log:
[2026-01-16T14:44:55.533-0800] [ERROR] trans_listen_address failed
[2026-01-16T14:44:55.533-0800] [CORE ] Failed to start xrdp daemon, possibly address already in use.

Network status:
tcp4       0      0  *.3389                 *.*                    LISTEN
(System daemon is listening, GUI app failed)
```

**Root Cause**:
- System daemons managed by launchd at:
  - /Library/LaunchDaemons/com.xrdp.xrdp.plist
  - /Library/LaunchDaemons/com.xrdp.sesman.plist
- These auto-restart when killed
- launchd launches system daemons first during boot
- System daemons win the race and bind port 3389
- GUI app cannot start because port is taken

**Solution** (requires sudo):
```bash
# Run the disable script:
./disable-system-xrdp.sh

# Or manually:
sudo launchctl unload /Library/LaunchDaemons/com.xrdp.xrdp.plist
sudo launchctl unload /Library/LaunchDaemons/com.xrdp.sesman.plist
sudo pkill -f "/usr/local/sbin/xrdp"

# To check system daemon logs (if connection attempts are happening):
sudo tail -50 /var/log/xrdp.log
```

## Recent Connection History

Last successful connections: 04:00 - 04:35 AM (when system daemon was running)
- TLS handshakes: ~70% success rate
- Some intermittent handshake failures (possibly client-side issues)
- No connections since 04:35 AM due to port conflict

## Code Changes Made

### 1. Enabled TLS Debug Logging
- File: [common/neutrinotls.c](common/neutrinotls.c#L7)
- Changed: `TLS13_DEBUG 0` → `TLS13_DEBUG 1`
- Purpose: Get detailed error messages for TLS handshake failures
- Built: /tmp/libcommon.0.dylib-debug (ready to deploy)

## What Happens After Port is Freed

Once the system daemons are disabled:
1. GUI app will be able to bind port 3389
2. Incoming connections from RemoteX will be accepted
3. TLS handshakes will be logged with full debug output
4. Any handshake errors will show detailed diagnostics

## To Deploy Debug Library (when ready to restart)

```bash
# Stop GUI app
pkill -f "/Applications/xrdp.app"

# Copy debug library
cp /tmp/libcommon.0.dylib-debug /Applications/xrdp.app/Contents/Resources/lib/libcommon.0.dylib

# Restart GUI app
open -a /Applications/xrdp.app
```

## Log Files to Monitor

- Main log: /Users/cyclic/Library/Logs/xrdp/xrdp.log
- Sesman log: /Users/cyclic/Library/Logs/xrdp/xrdp-sesman.log
- Debug output: stderr (redirected to /dev/null for GUI app currently)
