# xrdp macOS Packaging

This directory contains scripts to build, sign, and notarize macOS installer packages for xrdp with Apple Remote Desktop (ARD) authentication support.

## Prerequisites

### For Building
- macOS 11.0 (Big Sur) or later
- Xcode Command Line Tools: `xcode-select --install`
- OpenSSL 3.x (will be bundled in the package)
- Built xrdp from source (run `./bootstrap && ./configure && make`)

### For Code Signing (Optional but Recommended)
- Apple Developer Program membership ($99/year)
- Developer ID Application certificate
- Developer ID Installer certificate

### For Notarization (Required for Distribution)
- App Store Connect API key with Admin or App Manager role, OR
- Apple ID with app-specific password

## Quick Start

### 1. Build Package (Unsigned)
```bash
./build-pkg.sh 0.10.0
```

This creates: `xrdp-0.10.0-ard-macos-arm64.pkg` (or `x86_64` on Intel)

### 2. Build Package (Signed)
First, install your Developer ID certificates in Keychain Access, then:

```bash
./build-pkg.sh 0.10.0
```

The script auto-detects and uses certificates if available.

### 3. Notarize Package
After building a signed package:

```bash
# Option 1: Use App Store Connect API key (recommended)
export API_KEY_PATH=~/Downloads/AuthKey_XXXXXXXXXX.p8
export API_KEY_ID=XXXXXXXXXX
export ISSUER_ID=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx

./notarize-pkg.sh xrdp-0.10.0-ard-macos-arm64.pkg

# Option 2: Use Apple ID
./notarize-pkg.sh xrdp-0.10.0-ard-macos-arm64.pkg your@email.com TEAM_ID
```

### 4. Build and Notarize in One Step
```bash
NOTARIZE=yes ./build-pkg.sh 0.10.0
```

## Files

### Scripts
- **`build-pkg.sh`** - Main build script that creates the installer package
- **`notarize-pkg.sh`** - Submits package to Apple for notarization
- **`setup-signing.sh`** - Creates Developer ID certificate via API (advanced)
- **`preinstall`** - Runs before package installation
- **`postinstall`** - Runs after package installation, sets up LaunchDaemons

### Configuration
- **`com.xrdp.xrdp.plist`** - LaunchDaemon for xrdp server
- **`com.xrdp.sesman.plist`** - LaunchDaemon for session manager

### Documentation
- **`RELEASE_NOTES.md`** - Release notes template for GitHub releases

## Detailed Instructions

### Setting Up Code Signing

#### Option 1: Use Existing Certificates
If you already have Developer ID certificates:

1. Install them in Keychain Access (double-click .cer files)
2. Run `security find-identity -v -p codesigning` to verify
3. The build script will automatically detect and use them

#### Option 2: Create New Certificates Manually
1. Go to [Apple Developer Certificates](https://developer.apple.com/account/resources/certificates/list)
2. Create "Developer ID Application" certificate
3. Create "Developer ID Installer" certificate
4. Download and install both certificates

#### Option 3: Create Via API (Advanced)
```bash
export API_KEY_PATH=~/Downloads/AuthKey_XXXXXXXXXX.p8
export API_KEY_ID=XXXXXXXXXX
export ISSUER_ID=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
export CERT_EMAIL=your@email.com
export CERT_ORG="Your Organization"

./setup-signing.sh
```

**Note**: Only the Account Holder can create Developer ID certificates via API. Team members must use Option 1 or 2.

### Setting Up Notarization

#### Get App Store Connect API Key (Recommended)
1. Go to [App Store Connect API Keys](https://appstoreconnect.apple.com/access/integrations/api)
2. Create a key with "Admin" or "App Manager" role
3. Download the `.p8` file (you can only download once!)
4. Note the Key ID (10 characters) and Issuer ID (UUID)

```bash
export API_KEY_PATH=~/Downloads/AuthKey_XXXXXXXXXX.p8
export API_KEY_ID=XXXXXXXXXX
export ISSUER_ID=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
```

#### Get App-Specific Password (Alternative)
1. Go to [Apple ID Account](https://appleid.apple.com/account/manage)
2. Generate an app-specific password
3. Use it when prompted by the notarization script

### Package Contents

The built package installs:

```
/usr/local/
├── bin/
│   ├── xrdp-dis
│   ├── xrdp-genkeymap
│   ├── xrdp-keygen
│   └── xrdp-sesrun
├── sbin/
│   ├── xrdp
│   ├── xrdp-chansrv
│   └── xrdp-sesman
├── lib/xrdp/
│   ├── libssl.3.dylib          # Bundled OpenSSL
│   ├── libcrypto.3.dylib       # Bundled OpenSSL
│   ├── libcommon.0.dylib
│   ├── libxrdp.0.dylib
│   └── ... (other xrdp libraries)
├── libexec/xrdp/
│   └── xrdp-sesexec
└── share/xrdp/
    ├── licenses/               # All third-party licenses
    │   ├── XRDP-LICENSE.txt
    │   ├── OPENSSL-LICENSE.txt
    │   ├── NEUTRINORDP-LICENSE.txt
    │   └── TOMLC99-LICENSE.txt
    ├── fix-screen-recording.sh
    └── README.md

/etc/xrdp/
├── xrdp.ini
├── sesman.ini
└── km-*.toml                   # Keyboard mappings

/Library/LaunchDaemons/
├── com.xrdp.xrdp.plist
└── com.xrdp.sesman.plist
```

## Building for Distribution

For distribution to other users:

```bash
# 1. Build signed package
./build-pkg.sh 0.10.0

# 2. Notarize it
export API_KEY_PATH=~/Downloads/AuthKey_XXXXXXXXXX.p8
export API_KEY_ID=XXXXXXXXXX
export ISSUER_ID=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
./notarize-pkg.sh xrdp-0.10.0-ard-macos-arm64.pkg

# 3. Verify
spctl --assess --type install -vv xrdp-0.10.0-ard-macos-arm64.pkg
# Should show: accepted
#              source=Notarized Developer ID
```

## Post-Installation Setup

After installing the package, users need to:

1. **Enable Screen Sharing**
   - System Settings → General → Sharing → Screen Sharing

2. **Grant Screen Recording Permissions** (requires SIP disable)
   - Reboot to Recovery Mode (hold Power button)
   - Run: `csrutil disable`
   - Reboot normally
   - Run: `sudo /usr/local/share/xrdp/fix-screen-recording.sh`
   - Reboot to Recovery Mode again
   - Run: `csrutil enable` (optional, to re-enable SIP)
   - Reboot

3. **(Optional) Enable RemoteFX and Advanced Features**
   ```bash
   sudo /usr/local/share/xrdp/enable-remotefx.sh
   ```
   This enables:
   - RemoteFX (RFX) codec at 60 fps
   - H.264 video codec at 60 fps
   - Graphics Pipeline Extension (GFX)
   - Progressive rendering
   - Multi-monitor support
   - Optimized performance settings

4. **Connect**
   - Use any RDP client to connect to port 3389
   - Login with macOS username and password

## Troubleshooting

### Package Won't Install
```bash
# Check signature
pkgutil --check-signature xrdp-0.10.0-ard-macos-arm64.pkg

# Check Gatekeeper
spctl --assess --type install -vv xrdp-0.10.0-ard-macos-arm64.pkg
```

### Services Won't Start
```bash
# Check service status
sudo launchctl list | grep xrdp

# View logs
tail -f /var/log/xrdp.log
tail -f /var/log/xrdp-sesman.log

# Restart services
sudo launchctl stop com.xrdp.xrdp
sudo launchctl start com.xrdp.xrdp
```

### Library Path Issues
The package bundles OpenSSL at `/usr/local/lib/xrdp/`. If you see library loading errors:

```bash
# Check library paths
otool -L /usr/local/sbin/xrdp

# All OpenSSL references should point to /usr/local/lib/xrdp/libssl.3.dylib
```

## Environment Variables

### Build Script
- `VERSION` - Package version (default: from argument)
- `NOTARIZE` - Set to "yes" to auto-notarize after build

### Notarization Script
- `API_KEY_PATH` - Path to .p8 API key file
- `API_KEY_ID` - 10-character API key ID
- `ISSUER_ID` - UUID from App Store Connect
- `APPLE_ID` - Apple ID email (alternative to API key)
- `TEAM_ID` - Developer Team ID (alternative to API key)

### Code Signing Script
- `API_KEY_PATH` - Path to .p8 API key file
- `API_KEY_ID` - 10-character API key ID
- `ISSUER_ID` - UUID from App Store Connect
- `CERT_EMAIL` - Email for certificate
- `CERT_ORG` - Organization name for certificate

## License

The macOS packaging scripts are part of the xrdp project and are licensed under Apache License 2.0.

The package includes bundled third-party software with their own licenses:
- xrdp: Apache License 2.0
- OpenSSL: Apache License 2.0
- NeutrinoRDP: Apache License 2.0
- tomlc99: MIT License

All licenses are included in the package at `/usr/local/share/xrdp/licenses/`.
