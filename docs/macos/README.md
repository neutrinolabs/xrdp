# macOS Screen Sharing Support for xrdp

This directory contains helper scripts and documentation for connecting xrdp to macOS Screen Sharing using Apple Remote Desktop (ARD) authentication.

## Overview

xrdp can connect to macOS Screen Sharing (built-in VNC server) using the ARD authentication protocol (security type 30). This allows RDP clients to access macOS desktops through xrdp.

## Requirements

- macOS with Screen Sharing enabled (System Settings → General → Sharing → Screen Sharing)
- xrdp compiled with ARD authentication support
- SIP (System Integrity Protection) must be temporarily disabled to grant TCC permissions

## Setup Steps

### 1. Enable Screen Sharing on macOS

1. Open **System Settings → General → Sharing**
2. Enable **Screen Sharing**
3. Configure allowed users

### 2. Grant TCC Permissions (requires SIP disabled)

macOS requires Screen Recording and Accessibility permissions for the Screen Sharing daemon. Since `screensharingd` is a system daemon, these permissions must be added directly to the TCC database.

#### Disable SIP

**Apple Silicon Mac:**
1. Shut down your Mac completely
2. Press and hold the power button until "Loading startup options" appears
3. Click Options → Continue
4. Open Utilities → Terminal
5. Run: `csrutil disable`
6. Restart

**Intel Mac:**
1. Shut down your Mac
2. Turn on and immediately hold Cmd + R
3. Open Utilities → Terminal
4. Run: `csrutil disable`
5. Restart

#### Run the Permission Fix Script

```bash
sudo ./fix-screen-recording.sh
```

This script adds Screen Recording, Accessibility, PostEvent, and ListenEvent permissions for all Screen Sharing components.

#### Re-enable SIP (recommended)

After granting permissions and rebooting:
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

### 4. Connect

Use any RDP client to connect to xrdp on port 3389. Select the "macOS Desktop" session and enter your macOS username and password.

## Scripts

### fix-screen-recording.sh

Bash script that modifies the macOS TCC database to grant Screen Recording and Accessibility permissions to Screen Sharing components. Must be run with `sudo` after disabling SIP.

### test_vnc_pixels.py

Python test script that connects directly to macOS Screen Sharing using ARD authentication and checks if the screen content is being captured (non-black pixels). Useful for debugging permission issues.

Requirements:
```bash
pip3 install pycryptodome
```

Usage:
```bash
python3 test_vnc_pixels.py
```

## Troubleshooting

### Black screen (all pixels are black)

The Screen Sharing daemon doesn't have Screen Recording permission. Run `fix-screen-recording.sh` and reboot.

### Mouse/keyboard not working

The Screen Sharing daemon doesn't have Accessibility permission, or ARD privileges are not set to "Control". Run:

```bash
sudo /System/Library/CoreServices/RemoteManagement/ARDAgent.app/Contents/Resources/kickstart -configure -allowAccessFor -allUsers -privs -all -restart -agent
```

### "Observe mode" only (can see but not control)

ARD privileges are set to observe-only. Run the kickstart command above with `-privs -all` to enable full control.

### Authentication failure

- Verify username and password are correct
- Check that the user is allowed in Screen Sharing settings
- Ensure Screen Sharing is enabled

## Technical Details

### ARD Authentication Protocol (Security Type 30)

1. Server sends: generator (2 bytes) + key length (2 bytes) + prime (key_len bytes) + server public key (key_len bytes)
2. Client generates DH key pair, computes shared secret
3. Client derives AES key: MD5(shared_secret)
4. Client prepares credentials: 64 bytes username + 64 bytes password (null-terminated, random padding)
5. Client sends: AES-128-ECB encrypted credentials (128 bytes) + client public key (key_len bytes)
6. Server responds with security result (0 = success)

### TCC Services Required

- `kTCCServiceScreenCapture` - Screen Recording permission
- `kTCCServiceAccessibility` - Accessibility (input control) permission
- `kTCCServicePostEvent` - Permission to post input events
- `kTCCServiceListenEvent` - Permission to listen for input events
