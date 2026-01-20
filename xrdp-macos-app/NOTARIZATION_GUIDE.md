# XRDP macOS Notarization Guide

## Current Status

✅ **Completed:**
- App bundle signed with Developer ID Application certificate
- Installer packages created and signed with Developer ID Installer certificate
- DMG and ISO created and signed
- All files uploaded to GitHub release: [v0.10.0-macos-6-signed](https://github.com/Cyclic/xrdp/releases/tag/v0.10.0-macos-6-signed)

⏳ **Pending:**
- Apple Notarization
- Stapling notarization tickets

## Distribution Files

Located in: `/Users/cyclic/xrdp/xrdp-macos-app/dist/`

1. **XRDP-0.10.0-macos-5-neutrinossl-signed.pkg** (1.9 MB) - Signed installer ✅
2. **XRDP-0.10.0-macos-5-neutrinossl.dmg** (1.2 MB) - Signed disk image ✅
3. **XRDP-0.10.0-macos-5-neutrinossl.iso** (14 MB) - ISO image ✅
4. **XRDP-0.10.0-macos-5-neutrinossl.pkg** (984 KB) - Unsigned installer
5. **SHA256SUMS.txt** - Checksums

## Notarization Options

You have two methods available:

### Option 1: Using App Store Connect API Key (Recommended)

You have the API key at: `~/Downloads/AuthKey_N5Q2ZKTJB5.p8`

**Requirements:**
- API Key ID: `N5Q2ZKTJB5` ✅
- Issuer ID: **NEEDED** (UUID format from App Store Connect)

**How to get Issuer ID:**
1. Go to [App Store Connect](https://appstoreconnect.apple.com/)
2. Navigate to Users and Access → Keys
3. Find the Issuer ID at the top of the page

**Then run:**
```bash
cd /Users/cyclic/xrdp/xrdp-macos-app

# Update the ISSUER_ID in notarize-with-api-key.sh first!
# Then run:
./notarize-with-api-key.sh
```

### Option 2: Using App-Specific Password

**Requirements:**
1. Generate an app-specific password at [appleid.apple.com](https://appleid.apple.com)
2. Store it in keychain:
   ```bash
   xcrun notarytool store-credentials "notary-profile" \
     --apple-id "xrdp@neutrinos.app" \
     --team-id "H4PF9B4P9G" \
     --password "APP_SPECIFIC_PASSWORD"
   ```

**Then notarize:**
```bash
# Notarize PKG
xcrun notarytool submit dist/XRDP-0.10.0-macos-5-neutrinossl-signed.pkg \
  --keychain-profile "notary-profile" \
  --wait

# Staple PKG
xcrun stapler staple dist/XRDP-0.10.0-macos-5-neutrinossl-signed.pkg

# Notarize DMG
xcrun notarytool submit dist/XRDP-0.10.0-macos-5-neutrinossl.dmg \
  --keychain-profile "notary-profile" \
  --wait

# Staple DMG
xcrun stapler staple dist/XRDP-0.10.0-macos-5-neutrinossl.dmg
```

## Verification

After notarization, verify with:

```bash
# Verify PKG
spctl --assess --type install --verbose=4 dist/XRDP-0.10.0-macos-5-neutrinossl-signed.pkg

# Verify DMG
spctl --assess --type open --context context:primary-signature --verbose=4 dist/XRDP-0.10.0-macos-5-neutrinossl.dmg

# Check if notarization ticket is stapled
xcrun stapler validate dist/XRDP-0.10.0-macos-5-neutrinossl-signed.pkg
xcrun stapler validate dist/XRDP-0.10.0-macos-5-neutrinossl.dmg
```

## Re-uploading to GitHub

After notarization, update the GitHub release:

```bash
# Delete old assets
/tmp/gh_2.62.0_macOS_arm64/bin/gh release delete-asset v0.10.0-macos-6-signed \
  XRDP-0.10.0-macos-5-neutrinossl-signed.pkg \
  XRDP-0.10.0-macos-5-neutrinossl.dmg \
  --repo Cyclic/xrdp --yes

# Upload notarized files
/tmp/gh_2.62.0_macOS_arm64/bin/gh release upload v0.10.0-macos-6-signed \
  dist/XRDP-0.10.0-macos-5-neutrinossl-signed.pkg \
  dist/XRDP-0.10.0-macos-5-neutrinossl.dmg \
  --repo Cyclic/xrdp --clobber
```

## Notes

- Files are currently **signed but not notarized**
- They will work but macOS will show a security warning on first launch
- Users can approve via: Right-click → Open, or System Settings → Privacy & Security → "Open Anyway"
- Notarization is recommended but not required for distribution outside the Mac App Store
