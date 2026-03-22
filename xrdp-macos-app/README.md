# xrdp macOS App - Xcode Project

This Xcode project properly signs xrdp.app with your Developer ID certificate.

## Prerequisites

- Xcode installed
- Developer ID Application certificate: "Developer ID Application: Neutrino Labs, Inc. (F74NN74X3P)"
- Existing xrdp.app at `/Applications/xrdp.app` (for copying binaries and config)

## Building

### Option 1: Build via Xcode GUI

1. Open the project:
   ```bash
   open xrdp-macos-app/xrdp.xcodeproj
   ```

2. In Xcode:
   - Select **xrdp** scheme
   - Product → Clean Build Folder
   - Product → Build (⌘B)

3. The signed app will be at:
   ```
   ~/Library/Developer/Xcode/DerivedData/xrdp-*/Build/Products/Debug/xrdp.app
   ```

4. Copy to Applications:
   ```bash
   sudo rm -rf /Applications/xrdp.app
   sudo cp -R ~/Library/Developer/Xcode/DerivedData/xrdp-*/Build/Products/Debug/xrdp.app /Applications/
   ```

### Option 2: Build via Command Line

```bash
cd xrdp-macos-app
xcodebuild -project xrdp.xcodeproj -scheme xrdp -configuration Release clean build
```

The signed app will be at:
```
build/Release/xrdp.app
```

Install it:
```bash
sudo rm -rf /Applications/xrdp.app
sudo cp -R build/Release/xrdp.app /Applications/
```

## After Building

1. **Remove old TCC entries**:
   ```bash
   sudo sqlite3 "/Library/Application Support/com.apple.TCC/TCC.db" "DELETE FROM access WHERE service='kTCCServiceScreenCapture' AND client LIKE '%xrdp%';"
   ```

2. **Remove from System Settings** (if present):
   - System Settings → Privacy & Security → Screen Recording
   - Remove xrdp.app if listed

3. **Launch the app**:
   ```bash
   open /Applications/xrdp.app
   ```

4. **Start Server** - macOS will show permission dialog

5. **Grant Screen Recording permission** in System Settings

6. **Connect via RDP** - should work properly now!

## Verifying Code Signature

```bash
codesign -dv --verbose=4 /Applications/xrdp.app
codesign --verify --verbose=4 /Applications/xrdp.app
```

Should show:
- Authority=Developer ID Application: Neutrino Labs, Inc. (F74NN74X3P)
- TeamIdentifier=F74NN74X3P
- Signature valid

## Project Structure

- `xrdp-controller.m` - Menu bar controller app
- `Info.plist` - App bundle metadata
- `xrdp.entitlements` - Required permissions
- `xrdp.xcodeproj` - Xcode project with:
  - Team ID: F74NN74X3P
  - Code Sign Identity: Developer ID Application
  - Hardened Runtime enabled
  - Build phases to copy helpers/libs/config

## Copyright

Copyright ©2026 Neutrinos Software Corporation
Some portions Classify®
