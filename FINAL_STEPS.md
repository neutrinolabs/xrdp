# XRDP 0.10.0 macOS - Final Notarization Steps

## ✅ Completed

1. **App Built** - XRDP.app with Swift 6, custom icon, all caps branding
2. **App Signed** - Apple Distribution: Neutrinos Platforms, Inc. (H4PF9B4P9G)
3. **DMG Created** - Professional installer with custom background (326 KB)
4. **DMG Signed** - Apple Distribution: Neutrinos Platforms, Inc. (H4PF9B4P9G)
5. **Environment Configured** - All variables in ~/.zshrc
6. **Scripts Ready** - notarize-dmg.sh prepared

## 🔐 Get Issuer ID (Required)

The Issuer ID is a UUID that must be retrieved from App Store Connect:

1. Open: https://appstoreconnect.apple.com/access/integrations/api
2. Login with: xrdp@neutrinos.app
3. Look at the **top of the page** - the Issuer ID is displayed above the API keys table
4. Copy the UUID (format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx)

## 📝 Update Environment

Once you have the Issuer ID:

```bash
# Replace YOUR-ISSUER-ID-HERE with the actual UUID from App Store Connect
sed -i '' 's/REPLACE_WITH_ISSUER_ID_FROM_APP_STORE_CONNECT/YOUR-ISSUER-ID-HERE/' ~/.zshrc
source ~/.zshrc
```

## 🚀 Notarize

```bash
cd /Users/cyclic/xrdp

# Method 1: Use the automated script
./notarize-dmg.sh

# Method 2: Manual notarization
xcrun notarytool submit XRDP-0.10.0-macOS.dmg \
  --key /Users/cyclic/Downloads/AuthKey_N5Q2ZKTJB5.p8 \
  --key-id N5Q2ZKTJB5 \
  --issuer YOUR-ISSUER-ID-HERE \
  --wait

# Staple the notarization ticket
xcrun stapler staple XRDP-0.10.0-macOS.dmg

# Verify
spctl -a -vv -t install XRDP-0.10.0-macOS.dmg
```

## 📤 Upload to GitHub

```bash
# Upload the notarized DMG to the existing release
gh release upload v0.10.0-macos XRDP-0.10.0-macOS.dmg --clobber
```

## Current Status

- **App**: /Users/cyclic/xrdp/xrdp-macos-app/build/Debug/XRDP.app
- **DMG**: /Users/cyclic/xrdp/XRDP-0.10.0-macOS.dmg (326 KB)
- **Signed with**: Apple Distribution: Neutrinos Platforms, Inc. (H4PF9B4P9G)
- **Bundle ID**: remotex.app
- **Team ID**: H4PF9B4P9G

## ⚠️ Important

The DMG MUST be notarized before distribution. Without notarization, users will see:
- "App cannot be opened because the developer cannot be verified"
- macOS Gatekeeper warnings

With notarization, users can:
- Double-click the DMG and install without warnings
- macOS recognizes it as safe software
