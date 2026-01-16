# macOS Native Capture Module for xrdp

Copyright (C) 2026 Neutrinos Software Corporation
Some portions Classify®

This module provides native macOS screen capture for xrdp without requiring VNC (Screen Sharing).

## Features

- **Direct Screen Capture** - Uses macOS ScreenCaptureKit (macOS 12.3+)
- **No VNC Required** - Eliminates the need for Screen Sharing to be enabled
- **GPU Accelerated** - Efficient capture using IOSurface and CoreVideo
- **Damage Regions** - Only sends changed pixels (implemented in ScreenCaptureKit)
- **Native Input** - Mouse and keyboard events injected directly via CoreGraphics
- **Low Latency** - Direct capture without VNC protocol overhead

## Requirements

- macOS 12.3 (Monterey) or later
- Xcode Command Line Tools
- xrdp source code
- Screen Recording permission (TCC)

## Building

### Prerequisites

```bash
# Install Xcode Command Line Tools
xcode-select --install

# Build xrdp common library first
cd /Users/cyclic/xrdp
./bootstrap
./configure --prefix=/usr/local
make
```

### Build the Module

```bash
cd /Users/cyclic/xrdp/module_macos
make
```

### Install

```bash
sudo make install
```

This installs `libmacos.dylib` to `/usr/local/lib/xrdp/`

## Configuration

### 1. Update xrdp.ini

The macOS package installer automatically adds both session types to `/etc/xrdp/xrdp.ini`:

```ini
[macos_native]
name=macOS Desktop (Native - No VNC Required)
lib=libmacos.dylib
username=ask
password=ask

[macos]
name=macOS Desktop (VNC)
lib=libvnc.dylib
ip=127.0.0.1
port=5900
username=ask
password=ask
```

When connecting via RDP, select `macos_native` for native capture or `macos` for VNC fallback.

### 2. Grant Screen Recording Permission

The xrdp process needs Screen Recording permission:

```bash
# Add xrdp to TCC database (requires SIP disabled)
sudo sqlite3 /Library/Application\ Support/com.apple.TCC/TCC.db \
  "INSERT or REPLACE INTO access VALUES \
   ('kTCCServiceScreenCapture','$(which xrdp)',0,2,4,1,NULL,NULL,0,'UNUSED',NULL,0,1687538573);"

# Restart xrdp
sudo launchctl bootout system/com.xrdp.xrdp
sudo launchctl bootstrap system /Library/LaunchDaemons/com.xrdp.xrdp.plist
```

**Note:** On modern macOS with SIP enabled, you'll need to:
1. Boot to Recovery Mode
2. Disable SIP: `csrutil disable`
3. Reboot and add the permission
4. Re-enable SIP: `csrutil enable`

## Architecture

### Components

```
RDP Client
    ↓
xrdp (port 3389)
    ↓
libmacos.dylib
    ↓
ScreenCaptureKit API
    ↓
macOS Display Server
```

### How It Works

1. **Screen Capture**
   - Uses ScreenCaptureKit's SCStream for display capture
   - Captures at 30 FPS by default
   - Receives frames as CVPixelBuffer (GPU memory)
   - Damage tracking handled automatically

2. **Input Injection**
   - Mouse events converted to CGEvent
   - Posted to HID event tap
   - Keyboard events use CGKeyboard API
   - Full support for all mouse buttons and wheel

3. **Frame Delivery**
   - Frames received in background queue
   - Converted to RDP bitmap format
   - Sent via xrdp's server_paint_rect callback
   - Efficient pixel format conversion

### Key Files

- `macos_capture.h` - Module interface and structures
- `macos_capture.m` - ScreenCaptureKit integration
- `macos_input.m` - Mouse and keyboard handling
- `macos_module.c` - xrdp module interface implementation
- `Makefile` - Build configuration

## Advantages Over VNC

| Feature | Native Module | VNC-based |
|---------|--------------|-----------|
| Screen Sharing Required | ❌ No | ✅ Yes |
| Extra Protocol Layer | ❌ No | ✅ Yes (VNC) |
| GPU Acceleration | ✅ Yes | ⚠️  Limited |
| Damage Regions | ✅ Native | ⚠️  Protocol-level |
| Latency | ⬇️ Lower | ⬆️ Higher |
| CPU Usage | ⬇️ Lower | ⬆️ Higher |

## Troubleshooting

### Module Not Loading

Check xrdp logs:
```bash
tail -f /var/log/xrdp.log
```

Look for messages like:
- "macOS native capture module" - Module loaded
- "Initializing macOS native capture" - Capture initializing
- "Screen capture started successfully" - Capture active

### Black Screen

1. **Check TCC permissions:**
   ```bash
   sudo sqlite3 /Library/Application\ Support/com.apple.TCC/TCC.db \
     "SELECT * FROM access WHERE service='kTCCServiceScreenCapture';"
   ```

2. **Verify module is loaded:**
   ```bash
   grep "libmacos" /var/log/xrdp.log
   ```

3. **Check for errors:**
   ```bash
   grep -i error /var/log/xrdp.log
   ```

### Performance Issues

Adjust frame rate in `macos_capture.m`:

```objc
config.minimumFrameInterval = CMTimeMake(1, 60); // 60 FPS instead of 30
```

## Development

### Debugging

Enable debug logging in xrdp:
```bash
# Edit /etc/xrdp/xrdp.ini
[Logging]
LogLevel=DEBUG
```

### Testing

```bash
# Build and install
make clean && make && sudo make install

# Restart xrdp
sudo launchctl bootout system/com.xrdp.xrdp
sudo launchctl bootstrap system /Library/LaunchDaemons/com.xrdp.xrdp.plist

# Connect and check logs
tail -f /var/log/xrdp.log
```

## Future Enhancements

- [ ] H.264 hardware encoding integration
- [ ] Multi-monitor support
- [ ] Window-level capture (app sharing)
- [ ] Clipboard integration
- [ ] Audio capture/redirection
- [ ] Better scancode mapping for keyboard
- [ ] Dynamic resolution switching
- [ ] RemoteFX integration

## License

Apache License 2.0 - Same as xrdp

## Contributing

This module can be contributed back to the main xrdp project as an official macOS backend.

## Credits

Developed for xrdp macOS package (Build 4)
Using Apple's ScreenCaptureKit framework
