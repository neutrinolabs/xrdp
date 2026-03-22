# Notarization Instructions for XRDP-0.10.0-macOS.dmg

## Status

✅ **Release Created**: https://github.com/Cyclic/xrdp/releases/tag/v0.10.0-macos
✅ **DMG Uploaded**: XRDP-0.10.0-macOS.dmg (3.7 MB)
✅ **Code Signed**: Developer ID Application: Neutrino Labs, Inc. (F74NN74X3P)
⏳ **Notarization**: Requires app-specific password

## To Complete Notarization

### Step 1: Generate App-Specific Password

1. Go to https://appleid.apple.com/account/manage
2. Sign in with Apple ID: **xrdp@neutrinos.app**
3. In the "Security" section, click "Generate Password" under "App-Specific Passwords"
4. Label it: "XRDP Notarization"
5. Copy the generated password (format: xxxx-xxxx-xxxx-xxxx)

### Step 2: Store Password in Keychain

```bash
xcrun notarytool store-credentials "notarytool-xrdp" \
  --apple-id xrdp@neutrinos.app \
  --team-id F74NN74X3P \
  --password <paste-app-specific-password-here>
```

### Step 3: Submit DMG for Notarization

```bash
xcrun notarytool submit /Users/cyclic/xrdp/XRDP-0.10.0-macOS.dmg \
  --keychain-profile "notarytool-xrdp" \
  --wait
```

This will take 5-15 minutes. Wait for the response.

### Step 4: Staple the Notarization Ticket

Once approved:

```bash
xcrun stapler staple /Users/cyclic/xrdp/XRDP-0.10.0-macOS.dmg
xcrun stapler validate /Users/cyclic/xrdp/XRDP-0.10.0-macOS.dmg
```

### Step 5: Re-upload Notarized DMG

```bash
# Get the asset ID of the current DMG
ASSET_ID=$(curl -H "Authorization: token YOUR_GITHUB_TOKEN" \
  https://api.github.com/repos/Cyclic/xrdp/releases/278192327/assets | \
  grep -B 3 "XRDP-0.10.0-macOS.dmg" | grep '"id"' | head -1 | awk '{print $2}' | tr -d ',')

# Delete old asset
curl -X DELETE \
  -H "Authorization: token YOUR_GITHUB_TOKEN" \
  "https://api.github.com/repos/Cyclic/xrdp/releases/assets/${ASSET_ID}"

# Upload notarized DMG
curl -X POST \
  -H "Authorization: token YOUR_GITHUB_TOKEN" \
  -H "Content-Type: application/x-apple-diskimage" \
  "https://uploads.github.com/repos/Cyclic/xrdp/releases/278192327/assets?name=XRDP-0.10.0-macOS.dmg" \
  --data-binary "@/Users/cyclic/xrdp/XRDP-0.10.0-macOS.dmg"
```

## Current DMG Info

- **File**: XRDP-0.10.0-macOS.dmg
- **Size**: 3.7 MB (3,893,799 bytes)
- **SHA256**: `2b1ecd382a6e125c2f5ed213c8b7d5528ac0b1a74a27e4887d3415d385768ddd`
- **Signed**: ✅ Yes (Developer ID Application: Neutrino Labs, Inc.)
- **Notarized**: ⏳ Pending
- **Download**: https://github.com/Cyclic/xrdp/releases/download/v0.10.0-macos/XRDP-0.10.0-macOS.dmg

## Verification Commands

```bash
# Verify code signature
codesign -dvv /Users/cyclic/xrdp/XRDP-0.10.0-macOS.dmg

# Check notarization status (after stapling)
spctl -a -vv -t install /Users/cyclic/xrdp/XRDP-0.10.0-macOS.dmg

# View stapled ticket info
xcrun stapler validate -v /Users/cyclic/xrdp/XRDP-0.10.0-macOS.dmg
```

## Alternative: Notarize the App Bundle

You can also notarize XRDP.app directly before creating the DMG:

```bash
# Zip the app for notarization
cd /Users/cyclic/xrdp/xrdp-macos-app/build/Debug
ditto -c -k --keepParent XRDP.app XRDP.zip

# Submit for notarization
xcrun notarytool submit XRDP.zip \
  --keychain-profile "notarytool-xrdp" \
  --wait

# Staple to the app
xcrun stapler staple XRDP.app

# Then recreate the DMG with the notarized app
/tmp/create_dmg.sh
```

## Notes

- Notarization typically takes 5-15 minutes
- You'll receive an email at xrdp@neutrinos.app when complete
- The DMG is already code-signed and will work, but notarization adds Gatekeeper approval
- Users without notarization may see "App from unidentified developer" warning
