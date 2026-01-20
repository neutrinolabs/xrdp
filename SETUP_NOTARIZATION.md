# Notarization Setup for XRDP macOS

## Current Status

✅ **XRDP.app** - Built and signed with team H4PF9B4P9G
✅ **XRDP-0.10.0-macOS.dmg** - Created (3.7 MB)
✅ **API Key** - Found at /Users/cyclic/Downloads/AuthKey_N5Q2ZKTJB5.p8
✅ **Environment Variables** - Added to ~/.zshrc
⏳ **Issuer ID** - Needs to be retrieved from App Store Connect

## What's Been Configured

The following environment variables have been added to `~/.zshrc`:

```bash
export API_KEY_PATH="/Users/cyclic/Downloads/AuthKey_N5Q2ZKTJB5.p8"
export API_KEY_ID="N5Q2ZKTJB5"
export ISSUER_ID="REPLACE_WITH_ISSUER_ID_FROM_APP_STORE_CONNECT"
export APPLE_ID="xrdp@neutrinos.app"
export TEAM_ID="H4PF9B4P9G"
```

## Next Steps

### Step 1: Get the Issuer ID

The Issuer ID **cannot be extracted from the .p8 file** - you must get it from App Store Connect:

1. Open: https://appstoreconnect.apple.com/access/integrations/api
2. Login with: `xrdp@neutrinos.app`
3. Go to **Users and Access** → **Integrations** → **App Store Connect API**
4. The **Issuer ID** appears at the top of the page (above the API keys table)
5. It's a UUID in the format: `xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx`
6. Copy it

### Step 2: Update Environment Variable

Once you have the Issuer ID, run:

```bash
# Replace PASTE-YOUR-UUID-HERE with your actual Issuer ID
sed -i '' 's/REPLACE_WITH_ISSUER_ID_FROM_APP_STORE_CONNECT/PASTE-YOUR-UUID-HERE/' ~/.zshrc

# Reload configuration
source ~/.zshrc
```

### Step 3: Notarize the DMG

Run the notarization script:

```bash
cd /Users/cyclic/xrdp
./notarize-dmg.sh
```

This will:
1. Verify the DMG is signed
2. Submit to Apple for notarization (5-15 minutes)
3. Staple the notarization ticket to the DMG
4. Verify the notarization

### Step 4: Upload to GitHub

After notarization completes, upload the notarized DMG to the GitHub release:

```bash
# Get the asset ID
ASSET_ID=$(curl -s -H "Authorization: token $(gh auth token)" \
  https://api.github.com/repos/Cyclic/xrdp/releases/tags/v0.10.0-macos | \
  jq -r '.assets[] | select(.name=="XRDP-0.10.0-macOS.dmg") | .id')

# Delete old asset
curl -X DELETE \
  -H "Authorization: token $(gh auth token)" \
  "https://api.github.com/repos/Cyclic/xrdp/releases/assets/${ASSET_ID}"

# Upload notarized DMG
gh release upload v0.10.0-macos XRDP-0.10.0-macOS.dmg --clobber
```

## Alternative: Manual Notarization

If you prefer to notarize manually:

```bash
# Submit
xcrun notarytool submit XRDP-0.10.0-macOS.dmg \
  --key /Users/cyclic/Downloads/AuthKey_N5Q2ZKTJB5.p8 \
  --key-id N5Q2ZKTJB5 \
  --issuer YOUR-ISSUER-ID-HERE \
  --wait

# Staple
xcrun stapler staple XRDP-0.10.0-macOS.dmg

# Verify
spctl -a -vv -t install XRDP-0.10.0-macOS.dmg
```

## Troubleshooting

### "401 Unauthenticated" Error
- Double-check the Issuer ID is correct (must be copied from App Store Connect)
- Verify the API key file exists at the path specified
- Ensure the Key ID matches the filename (N5Q2ZKTJB5)

### "Invalid credentials" Error
- The Issuer ID is likely incorrect
- Log into App Store Connect and verify the UUID

### Notarization Takes Too Long
- Normal processing time is 5-15 minutes
- Use `--wait` flag to automatically poll for completion
- To check status manually: `xcrun notarytool history --key ... --key-id ... --issuer ...`

## References

- [Using App Store Connect API - fastlane docs](https://docs.fastlane.tools/app-store-connect-api/)
- [How to find Issuer ID on App Store Connect](https://help.apptile.com/en/articles/9479809)
- [App Store Connect API Key Configuration](https://www.revenuecat.com/docs/service-credentials/itunesconnect-app-specific-shared-secret/app-store-connect-api-key-configuration)
