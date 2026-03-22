# RemoteFX and Advanced Features Setup for xrdp macOS

## Summary

The new Build 3 package is ready with bundled NeutrinoRDP libraries. To enable RemoteFX and other advanced features, follow the steps below.

## Installation

The package at `/Users/cyclic/xrdp/packaging/macos/xrdp-0.10.0-ard-macos-arm64.pkg` is:
- ✅ Signed with Developer ID
- ✅ Notarized by Apple
- ✅ Hardened runtime enabled
- ✅ Self-contained with all dependencies bundled

To install, run:
```bash
sudo installer -pkg /Users/cyclic/xrdp/packaging/macos/xrdp-0.10.0-ard-macos-arm64.pkg -target /
```

## Current Status

### Installed Components
- xrdp server (with ARD authentication)
- xrdp-sesman (session manager)
- NeutrinoRDP libraries (for RDP client functionality)
- OpenSSL libraries
- VNC backend (`libvnc.dylib`)

### Current Configuration
The package includes default configuration with:
- **Security**: TLS negotiation with high encryption
- **Max color depth**: 32-bit
- **Fast-path**: Enabled for both input and output
- **Multi-monitor**: Enabled
- **Bitmap compression**: Enabled
- **Channels**: Enabled (clipboard, audio, etc.)

### Frame Capture Intervals (Current)
- H.264: 16ms (~62 fps)
- RFX (RemoteFX): 32ms (~31 fps)
- Normal: 40ms (~25 fps)

## Advanced Features Configuration

### 1. RemoteFX (RFX Codec)

RemoteFX is already configured in `/etc/xrdp/xrdp.ini`:
```ini
rfx_frame_interval=32
```

To optimize RemoteFX performance:
- Reduce interval for higher frame rate: `rfx_frame_interval=16` (60 fps)
- Increase for lower bandwidth: `rfx_frame_interval=64` (15 fps)

### 2. H.264 Video Codec

H.264 is configured and available:
```ini
h264_frame_interval=16
```

H.264 provides better compression than RemoteFX but requires:
- Client support for H.264 RDP extension
- More CPU on server side

### 3. Graphics Pipeline Extension (GFX)

To enable modern RDP 8.0+ graphics features, we need to:

1. Enable GFX in xrdp.ini (add to `[Globals]` section):
```ini
; Enable RDP 8.0+ Graphics Pipeline Extension
gfx=true
gfx_progressive=true
gfx_h264=true
gfx_rfx=true
```

2. Configure progressive rendering:
```ini
; Progressive quality levels (1-100)
gfx_progressive_quality_low=30
gfx_progressive_quality_medium=60
gfx_progressive_quality_high=90
```

### 4. Enhanced Audio/Video

Enable audio redirection and multimedia features:
```ini
[Globals]
; Audio redirection
audio_mode=2  ; 0=disabled, 1=local, 2=remote
; Video codec support
video_codec=h264
; Audio codec
audio_codec=opus
```

### 5. USB Redirection

Enable USB device redirection:
```ini
[Globals]
allow_channels=true
allowed_channels=cliprdr,rdpdr,rdpsnd,drdynvc
```

### 6. Smart Card Support

For smart card authentication:
```ini
[Globals]
allow_channels=true
smart_card_enabled=true
```

## Recommended Configuration for macOS

Create `/etc/xrdp/xrdp.ini.d/macos-optimized.ini` with:

```ini
[Globals]
; macOS Optimizations
max_bpp=32
use_fastpath=both
bitmap_compression=true
bulk_compression=true
new_cursors=true

; Modern Graphics Pipeline
gfx=true
gfx_progressive=true
gfx_h264=true
gfx_rfx=true

; Performance tuning
h264_frame_interval=16
rfx_frame_interval=16
normal_frame_interval=32

; Audio/Video
audio_mode=2
video_codec=h264

; Channels
allow_channels=true
allow_multimon=true
```

## Starting xrdp Service

After installation, the services should start automatically via LaunchDaemons:

```bash
# Check service status
sudo launchctl list | grep xrdp

# Manual start if needed
sudo launchctl load /Library/LaunchDaemons/com.xrdp.xrdp.plist
sudo launchctl load /Library/LaunchDaemons/com.xrdp.sesman.plist

# Verify services are running
ps aux | grep "[x]rdp"

# Check logs
tail -f /var/log/xrdp.log
tail -f /var/log/xrdp-sesman.log
```

## Testing RemoteFX

Connect with an RDP client that supports RemoteFX:
- Windows Remote Desktop Connection (mstsc.exe) - Windows 7+ with SP1
- FreeRDP with `--rfx` flag
- Microsoft Remote Desktop for macOS with quality set to "High"

Test command (from another machine):
```bash
xfreerdp /v:YOUR_MAC_IP:3389 /u:USERNAME /rfx /gfx:rfx /network:auto +clipboard
```

## Performance Monitoring

Monitor xrdp performance:
```bash
# Watch xrdp process
top -pid $(pgrep xrdp | head -1)

# Monitor network usage
nettop -p 3389

# View detailed logs with performance metrics
tail -f /var/log/xrdp.log | grep -E "(fps|bps|codec)"
```

## Troubleshooting

### Service won't start
```bash
# Check library paths
otool -L /usr/local/sbin/xrdp

# All libraries should point to /usr/local/lib/xrdp/*
# No relative paths or missing libraries

# Check process errors
sudo launchctl list | grep xrdp
cat /var/log/xrdp.err
```

### RemoteFX not working
1. Ensure client supports RemoteFX (RDP 7.1+)
2. Check `gfx=true` in xrdp.ini
3. Verify codec is being used: check logs for "rfx" or "h264"

### Poor performance
1. Reduce frame intervals for higher FPS
2. Enable `use_fastpath=both`
3. Consider network bandwidth and latency
4. Use H.264 instead of RFX for high-latency connections

## Release Information

- **Package**: xrdp-0.10.0-ard-macos-arm64.pkg (Build 3)
- **Release**: https://github.com/Cyclic/xrdp/releases/tag/v0.10.0-macos-3
- **PR**: #3702 (pending)
- **Fixes**: Issue #3696 (macOS hardcoded library paths)

## Next Steps

1. Install the package
2. Apply optimized configuration
3. Start/restart xrdp services
4. Test RemoteFX connection
5. Monitor performance and adjust settings

---

For questions or issues, see the main documentation at `/usr/local/share/xrdp/README.md`
