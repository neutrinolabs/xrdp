# XRDP 0.10.0 for macOS - TLS 1.3 Edition

## 🎉 What's New

This release brings a complete rewrite of the macOS menu bar app and includes critical TLS fixes for the XRDP server.

### ✨ Highlights

- **Swift 6 Menu Bar App**: Complete rewrite from Objective-C to Swift 6 with SwiftUI
- **Custom App Icon**: New atomic symbol icon matching the menu bar branding
- **TLS 1.3 Fix**: Fixed critical decryption bug in NeutrinoTLS implementation
- **Auto-Start**: Server automatically starts when the app launches
- **Native macOS Integration**: Uses SwiftUI MenuBarExtra for native feel

### 🔧 Menu Bar App Features

- **Server Controls**: Start/Stop XRDP server with one click
- **Live Status**: Shows server running state and connection count
- **Notifications**: Get notified of new connections and server events
- **About Dialog**: Quick access to version and license information
- **Professional UI**: All caps "XRDP" branding throughout

### 🐛 Bug Fixes

- **TLS Decryption**: Fixed server using wrong keys after TLS handshake
  - Added `is_server` flag to distinguish server/client mode
  - Server now correctly uses CLIENT keys to decrypt CLIENT data
  - Resolves "Decryption failed (bad MAC)" errors

- **Menu Hang Fix**: Eliminated menu freezing issues
  - Changed `isServerRunning` to computed property
  - Disabled background connection monitoring
  - All state updates are now non-blocking

### 📦 Installation

1. Download `XRDP-0.10.0-macOS.dmg`
2. Open the DMG file
3. Drag XRDP.app to the Applications folder
4. Launch XRDP from Applications
5. The server will auto-start and listen on port 3389

### 🔒 Security

- Code signed with Developer ID: Neutrino Labs, Inc.
- Requests firewall permissions for port 3389
- Uses TLS 1.3 with ChaCha20-Poly1305 encryption
- Hardened runtime enabled

### 🏗️ Technical Details

- **Swift Version**: 6.0
- **Minimum macOS**: 14.0 (Sonoma)
- **Architecture**: Universal Binary (Apple Silicon + Intel)
- **TLS**: NeutrinoTLS 1.3 with X25519 + ChaCha20-Poly1305
- **App Size**: 3.7 MB (DMG)

### 📝 Commits in This Release

1. Rename app to XRDP.app and add system permission descriptions (351ba8df)
2. Add atomic symbol app icon for macOS (eb89bc8f)
3. Capitalize XRDP consistently in menu bar app UI (ab0943b2)
4. Fix menu bar hang by making server state computed (acb9b5fa)
5. Fix TLS decryption and convert menu bar app to Swift 6 (dcd2f7bd)
6. Convert macOS menu bar app to SwiftUI with full async support (9f6d13c1)
7. Add Swift 6 version of xrdp menu bar app (d0f87de1)

### 🙏 Credits

Built with NeutrinoTLS and XRDP open source projects.

Copyright ©2026 Neutrinos Software Corporation. Some portions Classify®

---

**Download**: XRDP-0.10.0-macOS.dmg (3.7 MB)
**SHA256**: (to be calculated)

To create the release:
1. Go to https://github.com/Cyclic/xrdp/releases/new
2. Tag: v0.10.0-macos
3. Title: XRDP 0.10.0 for macOS with NeutrinoTLS
4. Upload: XRDP-0.10.0-macOS.dmg
5. Copy/paste these release notes
