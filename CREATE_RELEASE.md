# Create GitHub Release for v0.10.0-macos-5-neutrinossl

## Release Information

**Tag:** `v0.10.0-macos-5-neutrinossl` (already pushed to GitHub)
**Repository:** https://github.com/Cyclic/xrdp
**Title:** macOS XRDP v0.10.0 - NeutrinoSSL Integration

## Release Notes

```markdown
# macOS XRDP with NeutrinoSSL/TLS/Crypto - OpenSSL-free build

This release integrates **NeutrinoSSL, NeutrinoTLS, and NeutrinoCrypto** to completely eliminate the OpenSSL dependency on macOS, preventing PAC (Pointer Authentication Code) crashes on Apple Silicon.

## Key Features

✅ **Full NeutrinoSSL/TLS/Crypto integration** for macOS builds
✅ **Pure C crypto primitives** (RC4, MD5, SHA-1, HMAC, DES3, AES-128)
✅ **TLS-only security** (`security_layer=tls`) - no legacy RDP encryption
✅ **System font support** (no proprietary bitmap fonts required)
✅ **EntanglementGuestTools** distribution files included (DMG, ISO, PKG)
✅ **Optimized for Apple Silicon** (no OpenSSL, no RSA operations)

## Technical Details

- **NeutrinoTLS**: TLS 1.3 with ChaCha20-Poly1305, X25519 ECDH, SHA-256
- **Conditional compilation** via `USE_NEUTRINOSSL` preprocessor flag
- All crypto operations use **NeutrinoCrypto** instead of OpenSSL on macOS
- Threading model instead of fork for macOS compatibility

## Installation

Download one of the following:
- **EntanglementGuestTools.dmg** - macOS installer disk image (recommended)
- **EntanglementGuestTools.iso** - ISO for VM mounting
- **EntanglementGuestTools-signed.pkg** - Signed installer package
- **EntanglementGuestTools.pkg** - Unsigned installer package

## What Changed

This eliminates the need for RSA operations by using `security_layer=tls` exclusively. All connections use NeutrinoTLS with pure C crypto, making XRDP fully compatible with Apple Silicon's enhanced security features.

## Files to Upload

1. `xrdp-macos-app/EntanglementGuestTools.dmg` (135 KB)
2. `xrdp-macos-app/EntanglementGuestTools.iso` (900 KB)
3. `xrdp-macos-app/EntanglementGuestTools-signed.pkg` (61 KB)
4. `xrdp-macos-app/EntanglementGuestTools.pkg` (63 KB)

---

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>
```

## Steps to Create Release via GitHub Web Interface

1. Go to https://github.com/Cyclic/xrdp/releases/new
2. Select tag: `v0.10.0-macos-5-neutrinossl`
3. Set title: `macOS XRDP v0.10.0 - NeutrinoSSL Integration`
4. Copy the release notes above into the description
5. Upload the following files from `xrdp-macos-app/`:
   - EntanglementGuestTools.dmg
   - EntanglementGuestTools.iso
   - EntanglementGuestTools-signed.pkg
   - EntanglementGuestTools.pkg
6. Click "Publish release"

## Alternative: Use GitHub CLI (if installed)

If you have `gh` CLI installed, run this command:

```bash
cd /Users/cyclic/xrdp

gh release create v0.10.0-macos-5-neutrinossl \
  --repo Cyclic/xrdp \
  --title "macOS XRDP v0.10.0 - NeutrinoSSL Integration" \
  --notes-file CREATE_RELEASE.md \
  xrdp-macos-app/EntanglementGuestTools.dmg \
  xrdp-macos-app/EntanglementGuestTools.iso \
  xrdp-macos-app/EntanglementGuestTools-signed.pkg \
  xrdp-macos-app/EntanglementGuestTools.pkg
```

## Summary

✅ All code changes committed (48 total commits on branch)
✅ Changes pushed to fork: https://github.com/Cyclic/xrdp/tree/fix/macos-pkg-bundle-openssl
✅ Release tag created and pushed: `v0.10.0-macos-5-neutrinossl`
✅ EntanglementGuestTools files added to repository
✅ Ready to create GitHub release (manual step via web UI)

The new XRDP build is fully configured with:
- NeutrinoSSL/TLS/Crypto integration (no OpenSSL)
- TLS-only security (no RSA operations needed)
- System font support
- Properly built and signed with Xcode
- Installed to /Applications/XRDP.app
