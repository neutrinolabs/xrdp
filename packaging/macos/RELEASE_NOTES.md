# xrdp v0.10.0 for macOS (Signed Release)

## Overview
This is a fully signed macOS installer package for xrdp with Apple Remote Desktop (ARD) authentication support.

## What's New in This Release

### Code Signing
- ✅ All binaries and libraries signed with **Developer ID Application** certificate
- ✅ Package signed with **Developer ID Installer** certificate
- ✅ Trusted timestamp included for long-term validity
- ✅ Installs without "unidentified developer" warnings

### Fixed Issues
- **#3696**: Fixed hardcoded OpenSSL library paths that prevented package from working on other systems
- OpenSSL 3.x libraries are now bundled with the package at `/usr/local/lib/xrdp/`
- All library references rewritten to use bundled libraries using `install_name_tool`

### Improvements
- Auto-detection of OpenSSL location during build
- Automatic code signing when certificates are available
- Complete third-party license files included (OpenSSL, xrdp, NeutrinoRDP, tomlc99)
- LaunchDaemon configurations include proper `DYLD_LIBRARY_PATH` settings

## Installation

### Requirements
- macOS 11.0 (Big Sur) or later
- Apple Silicon (ARM64) Mac

### Install Steps
1. Download `xrdp-0.10.0-ard-macos-arm64.pkg`
2. Run the installer:
   ```bash
   sudo installer -pkg xrdp-0.10.0-ard-macos-arm64.pkg -target /
   ```

### Post-Installation Setup
After installation, follow these steps to enable xrdp:

1. **Enable Screen Sharing**
   - Go to System Settings → General → Sharing
   - Enable "Screen Sharing"

2. **Grant Screen Recording Permissions** (requires SIP disable)
   - Reboot to Recovery Mode (hold Power button on Apple Silicon)
   - Run: `csrutil disable`
   - Reboot normally
   - Run: `sudo /usr/local/share/xrdp/fix-screen-recording.sh`
   - Reboot to Recovery Mode again
   - Run: `csrutil enable` (optional, to re-enable SIP)
   - Reboot

3. **Connect**
   - Use any RDP client to connect to port 3389
   - Login with your macOS username and password

## What's Included

### Binaries (all signed)
- xrdp server (`/usr/local/sbin/xrdp`)
- xrdp-sesman session manager
- xrdp-chansrv channel server
- Supporting utilities

### Libraries (all signed)
- Bundled OpenSSL 3.x libraries
- xrdp core libraries
- NeutrinoRDP module

### Configuration
- LaunchDaemon plists for auto-start
- Default configuration files
- Setup and helper scripts

### Documentation
- Complete license files for all third-party software
- Setup instructions and troubleshooting guide
- Man pages

## Verification

Verify package signature:
```bash
pkgutil --check-signature xrdp-0.10.0-ard-macos-arm64.pkg
```

Expected output:
```
Package "xrdp-0.10.0-ard-macos-arm64.pkg":
   Status: signed by a developer certificate issued by Apple for distribution
   Certificate Chain:
    1. Developer ID Installer: Neutrino Labs, Inc.
    2. Developer ID Certification Authority
    3. Apple Root CA
```

## Architecture
This package is built for **Apple Silicon (ARM64)** Macs only. Intel (x86_64) users should build from source.

## Building from Source
See the [macOS documentation](https://github.com/neutrinolabs/xrdp/tree/devel/docs/macos) for instructions on building xrdp for macOS.

## License
- **xrdp**: Apache License 2.0
- **OpenSSL**: Apache License 2.0
- **NeutrinoRDP**: Apache License 2.0
- **tomlc99**: MIT License

All license files are included at `/usr/local/share/xrdp/licenses/`

## Support
For issues and support, please visit:
- GitHub Issues: https://github.com/neutrinolabs/xrdp/issues
- Documentation: https://github.com/neutrinolabs/xrdp/tree/devel/docs/macos
