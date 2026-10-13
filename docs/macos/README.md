# macOS Screen Sharing Support for xrdp

This directory contains helper scripts and documentation for connecting xrdp to macOS Screen Sharing using Apple Remote Desktop (ARD) authentication.

## Overview

xrdp can connect to macOS Screen Sharing (built-in VNC server) using the ARD authentication protocol (security type 30). This allows RDP clients to access macOS desktops through xrdp.

## Requirements

- macOS with Screen Sharing enabled (System Settings → General → Sharing → Screen Sharing)
- xrdp compiled with ARD authentication support
- SIP (System Integrity Protection) must be temporarily disabled to grant TCC permissions
- Python 3 with pycryptodome (for test script only)

## Setup Steps

### 1. Enable Screen Sharing on macOS

1. Open **System Settings → General → Sharing**
2. Enable **Screen Sharing**
3. Click the (i) info button and configure allowed users
4. Note: VNC viewers using password-only authentication should set a VNC password here

### 2. Grant TCC Permissions (requires SIP disabled)

macOS requires Screen Recording and Accessibility permissions for the Screen Sharing daemon. Since `screensharingd` is a system daemon, these permissions must be added directly to the TCC database.

**Important:** On macOS 13+ (Ventura and later), TCC entries require proper code signature requirements (`csreq`) to be validated. The `fix-screen-recording.sh` script handles this automatically.

#### Disable SIP

**Apple Silicon Mac (M1/M2/M3/M4):**
1. Shut down your Mac completely
2. Press and hold the power button until "Loading startup options" appears
3. Click **Options** → **Continue**
4. Select your user and enter password if prompted
5. Open **Utilities → Terminal**
6. Run: `csrutil disable`
7. Restart

**Intel Mac:**
1. Shut down your Mac
2. Turn on and immediately hold **Cmd + R**
3. Open **Utilities → Terminal**
4. Run: `csrutil disable`
5. Restart

**Virtualization Framework VMs (Parallels, UTM, etc.):**
- Parallels: **Actions → Start in Recovery Mode** or hold Cmd+R during boot
- UTM: Edit VM settings → System → enable "Boot into Recovery"
- VMware Fusion: **Virtual Machine → Start in Recovery Mode**

#### Run the Permission Fix Script

```bash
sudo ./fix-screen-recording.sh
```

This script:
- Generates proper code signature requirements for Apple system binaries
- Adds Screen Recording (`kTCCServiceScreenCapture`) permissions
- Adds Accessibility (`kTCCServiceAccessibility`) permissions
- Adds PostEvent and ListenEvent permissions for input control
- Creates a backup of the TCC database before modification

**Important:** Reboot after running the script for changes to take effect.

#### Enable Full Control Privileges

After rebooting, enable ARD control privileges:

```bash
sudo /System/Library/CoreServices/RemoteManagement/ARDAgent.app/Contents/Resources/kickstart \
    -configure -allowAccessFor -allUsers -privs -all -restart -agent
```

This grants all users full control access (not just observe-only).

#### Re-enable SIP (recommended)

After granting permissions and verifying everything works:
1. Boot to Recovery Mode again
2. Run: `csrutil enable`
3. Restart

### 3. Configure xrdp

Add a macOS session to `/etc/xrdp/xrdp.ini`:

```ini
[macos]
name=macOS Desktop
lib=libvnc.dylib
ip=127.0.0.1
port=5900
username=ask
password=ask
```

**Note:** After running `make install`, the xrdp.ini file may be overwritten. Re-add the `[macos]` section if needed.

### 4. Start xrdp

```bash
sudo /usr/local/sbin/xrdp --nodaemon &
sudo /usr/local/sbin/xrdp-sesman --nodaemon &
```

Or if you've set up launchd services:
```bash
sudo launchctl load /Library/LaunchDaemons/xrdp.plist
sudo launchctl load /Library/LaunchDaemons/xrdp-sesman.plist
```

### 5. Connect

Use any RDP client to connect to xrdp on port 3389. Select the "macOS Desktop" session and enter your macOS username and password.

## Scripts

### fix-screen-recording.sh

Bash script that modifies the macOS TCC database to grant Screen Recording and Accessibility permissions to Screen Sharing components.

**Features:**
- Checks SIP status before proceeding
- Creates timestamped backup of TCC database
- Generates proper `csreq` code signature requirements for system binaries
- Adds permissions for multiple bundle IDs and binary paths
- Verifies entries after insertion

**Usage:**
```bash
sudo ./fix-screen-recording.sh
```

**Must be run after disabling SIP.**

### test_vnc_pixels.py

Python test script that connects directly to macOS Screen Sharing using ARD authentication and checks if the screen content is being captured (non-black pixels). Useful for debugging permission issues.

**Features:**
- Full ARD authentication implementation
- Detailed connection diagnostics
- Pixel analysis to verify screen capture is working
- Prompts for credentials (no hardcoded passwords)
- Supports environment variables for automation

**Requirements:**
```bash
pip3 install pycryptodome
```

**Usage:**
```bash
# Interactive (prompts for credentials)
python3 test_vnc_pixels.py

# With command-line arguments
python3 test_vnc_pixels.py -u username -H 127.0.0.1 -p 5900

# With environment variables
VNC_USERNAME=user VNC_PASSWORD=pass python3 test_vnc_pixels.py
```

**Options:**
- `-u, --username` - Username (or set `VNC_USERNAME` env var)
- `-H, --host` - VNC host (default: 127.0.0.1)
- `-p, --port` - VNC port (default: 5900)

## Troubleshooting

### Black screen (all pixels are black)

**Cause:** Screen Sharing daemon doesn't have Screen Recording permission.

**Solution:**
1. Ensure SIP is disabled: `csrutil status`
2. Run `sudo ./fix-screen-recording.sh`
3. **Reboot** (required for TCC changes to take effect)
4. Test with: `python3 test_vnc_pixels.py`

### Mouse/keyboard not working

**Cause:** Missing Accessibility permission or ARD privileges set to "Observe only".

**Solution:**
1. Run the fix script to add Accessibility permissions:
   ```bash
   sudo ./fix-screen-recording.sh
   ```

2. Enable full control with kickstart:
   ```bash
   sudo /System/Library/CoreServices/RemoteManagement/ARDAgent.app/Contents/Resources/kickstart \
       -configure -allowAccessFor -allUsers -privs -all -restart -agent
   ```

3. Reboot and reconnect

### "Observe mode" indicator (purple icon, can see but not control)

**Cause:** ARD privileges are set to observe-only.

**Solution:** Run the kickstart command above with `-privs -all` to enable full control.

### Authentication failure ("Authentication or authorisation failure")

**Possible causes and solutions:**

1. **Wrong credentials:** Verify username and password are correct for a local macOS user

2. **User not allowed:** Check System Settings → Sharing → Screen Sharing → allowed users

3. **Screen Sharing disabled:** Ensure Screen Sharing is enabled in System Settings

4. **ARD authentication issue:** The server might not support ARD auth. Check VNC logs.

### Connection refused on port 5900

**Cause:** Screen Sharing is not running.

**Solution:**
1. Enable Screen Sharing in System Settings → Sharing
2. Verify it's listening: `netstat -an | grep 5900`

### Permission changes not taking effect

**Cause:** TCC daemon caches permissions.

**Solution:** Reboot the Mac. TCC changes require a reboot to take effect.

## Technical Details

### ARD Authentication Protocol (Security Type 30)

The Apple Remote Desktop authentication protocol uses Diffie-Hellman key exchange with AES encryption:

1. **Server → Client:**
   - Generator (2 bytes, big-endian)
   - Key length (2 bytes, big-endian, typically 128 = 1024-bit DH)
   - Prime (key_len bytes)
   - Server public key (key_len bytes)

2. **Client computes:**
   - Generates DH private key
   - Computes client public key: `g^private mod prime`
   - Computes shared secret: `server_pubkey^private mod prime`
   - Derives AES key: `MD5(shared_secret)`

3. **Client prepares credentials:**
   - 64 bytes for username (null-terminated, random padding after)
   - 64 bytes for password (null-terminated, random padding after)
   - Total: 128 bytes

4. **Client → Server:**
   - AES-128-ECB encrypted credentials (128 bytes)
   - Client public key (key_len bytes)

   **Important:** The order is encrypted credentials FIRST, then public key.

5. **Server → Client:**
   - Security result (4 bytes): 0 = success, non-zero = failure

### TCC Services Required

The following TCC (Transparency, Consent, and Control) services must be granted:

| Service | Purpose |
|---------|---------|
| `kTCCServiceScreenCapture` | Screen Recording - capture screen content |
| `kTCCServiceAccessibility` | Accessibility - control mouse/keyboard |
| `kTCCServicePostEvent` | Post input events to the system |
| `kTCCServiceListenEvent` | Listen for input events |

### Code Signature Requirements

On macOS 13+, TCC entries for system binaries require proper `csreq` (code signature requirement) blobs. The fix script generates these using:

```bash
codesign -dr - /path/to/binary
csreq -r- -b /tmp/csreq.bin
```

The resulting binary blob is stored in the TCC database's `csreq` column.

### Related Files and Paths

| Path | Description |
|------|-------------|
| `/Library/Application Support/com.apple.TCC/TCC.db` | System TCC database |
| `~/Library/Application Support/com.apple.TCC/TCC.db` | User TCC database |
| `/System/Library/CoreServices/RemoteManagement/screensharingd.bundle` | Screen Sharing daemon |
| `/System/Library/CoreServices/RemoteManagement/ARDAgent.app` | ARD Agent |
| `/Library/Preferences/com.apple.RemoteManagement.plist` | ARD configuration |
| `/etc/xrdp/xrdp.ini` | xrdp configuration |
| `/etc/xrdp/sesman.ini` | xrdp session manager configuration |

### macOS-Specific Configuration Notes

**sesman.ini:** On macOS, change `SessionSockdirGroup` from `root` to `wheel`:
```ini
SessionSockdirGroup=wheel
```

**Library paths:** After `make install`, you may need to fix dylib paths:
```bash
install_name_tool -change /path/to/old/lib.dylib /usr/local/lib/xrdp/lib.dylib /usr/local/sbin/xrdp
```

## Tested Configurations

- macOS 26.2 (Tahoe) on Apple Silicon VM (Virtualization Framework)
- xrdp compiled from source with OpenSSL 3.x
- RDP clients: Microsoft Remote Desktop, Remmina

## References

- [Apple Remote Desktop Authentication](https://cafbit.com/post/apple_remote_desktop_quirks/) - Protocol details
- [gtk-vnc ARD implementation](https://github.com/jwendell/gtk-vnc/blob/master/src/vncconnection.c) - Reference implementation
- [RFB Protocol Specification](https://github.com/rfbproto/rfbproto) - VNC/RFB protocol
