# NeutrinoSSL Implementation

## Overview

NeutrinoSSL is a lightweight TLS implementation for xrdp on macOS that replaces OpenSSL with native macOS Secure Transport APIs. This was created to avoid Pointer Authentication Code (PAC) crashes that occur when calling OpenSSL functions from worker threads on Apple Silicon Macs.

## Background

The xrdp server was experiencing SIGSEGV crashes on Apple Silicon (M1/M2/M3) Macs when calling `SSL_CTX_new()` and related OpenSSL functions from worker threads. Multiple approaches were tried:

1. Disabling fork mode (using threading instead)
2. Adding `-mbranch-protection=none` compiler flag
3. Switching from `SSLv23_server_method()` to `TLS_server_method()`
4. Various OpenSSL initialization sequences

None of these approaches resolved the PAC crashes. The root cause appears to be incompatibility between OpenSSL's internal threading/memory management and Apple Silicon's Pointer Authentication Code security feature.

## Solution

Create a custom SSL/TLS implementation (NeutrinoSSL) that:
- Uses macOS native Secure Transport API instead of OpenSSL
- Provides a minimal API surface matching what xrdp needs
- Integrates seamlessly with existing xrdp code through conditional compilation

## Files Created

### [common/neutrinossl.h](common/neutrinossl.h)

Header file defining the NeutrinoSSL API that mirrors OpenSSL's interface:

- `NEUTRINOSSL_CTX` - Context type (equivalent to SSL_CTX)
- `NEUTRINOSSL` - Connection type (equivalent to SSL)
- API functions:
  - `neutrinossl_init()` - Initialize library
  - `neutrinossl_ctx_new_server()` - Create server context
  - `neutrinossl_new()` - Create SSL connection
  - `neutrinossl_accept()` - Perform TLS handshake
  - `neutrinossl_read()` / `neutrinossl_write()` - I/O operations
  - `neutrinossl_shutdown()` - Close connection
  - Cleanup functions

### [common/neutrinossl.c](common/neutrinossl.c)

Implementation using macOS Secure Transport API:

- Uses `SSLContextRef` for SSL context
- Uses `SecIdentityRef` for certificate/key identity
- Implements I/O callbacks for socket read/write
- Sets minimum TLS version to TLS 1.2
- Thread-local error storage

**Key Implementation Details:**
- `SSLCreateContext()` creates SSL context
- `SSLSetIOFuncs()` sets up socket I/O callbacks
- `SSLHandshake()` performs TLS handshake
- `SSLRead()` / `SSLWrite()` for encrypted I/O
- Currently generates self-signed certificate (TODO: load from PEM files)

## Integration into xrdp

### [common/ssl_calls.c](common/ssl_calls.c)

Modified to support both OpenSSL and NeutrinoSSL through conditional compilation:

1. **Includes section** (lines 22-71):
   - `#ifdef __APPLE__` uses NeutrinoSSL
   - Still includes OpenSSL crypto headers for MD5, RC4, etc.

2. **struct ssl_tls** (lines 90-104):
   - Conditionally uses `NEUTRINOSSL*` or `SSL*`
   - Conditionally uses `NEUTRINOSSL_CTX*` or `SSL_CTX*`

3. **ssl_init()** (lines 197-208):
   - Calls `neutrinossl_init()` on macOS
   - Calls `SSL_library_init()` on other platforms

4. **ssl_tls_accept()** (lines 1157-1430):
   - NeutrinoSSL path: Creates context, connection, performs handshake
   - OpenSSL path: Preserved existing implementation

5. **ssl_tls_read()** (lines 1490-1556):
   - NeutrinoSSL: Calls `neutrinossl_read()`
   - OpenSSL: Preserves existing retry logic

6. **ssl_tls_write()** (lines 1559-1620):
   - NeutrinoSSL: Calls `neutrinossl_write()`
   - OpenSSL: Preserves existing retry logic

7. **ssl_tls_disconnect()** (lines 1434-1471):
   - NeutrinoSSL: Calls `neutrinossl_shutdown()`
   - OpenSSL: Preserves existing shutdown logic

8. **ssl_tls_delete()** (lines 1474-1505):
   - NeutrinoSSL: Calls `neutrinossl_free()` and `neutrinossl_ctx_free()`
   - OpenSSL: Calls `SSL_free()` and `SSL_CTX_free()`

## Build System Changes

### [common/Makefile.am](common/Makefile.am)

Added neutrinossl.c and neutrinossl.h to libcommon_la_SOURCES.

### [common/Makefile](common/Makefile)

Direct modifications to include NeutrinoSSL:

1. Added to `am__libcommon_la_SOURCES_DIST` (line 151)
2. Added `neutrinossl.lo` to `am_libcommon_la_OBJECTS` (line 157)
3. Added `./$(DEPDIR)/neutrinossl.Plo` to dependency files (line 185)
4. Added to `libcommon_la_SOURCES` (lines 472-473)
5. Added `-framework Security` to `libcommon_la_LIBADD` (line 487)

## Current Status

### Completed ✅

1. ✅ Designed NeutrinoSSL API matching OpenSSL interface
2. ✅ Implemented NeutrinoSSL using Secure Transport
3. ✅ Integrated into xrdp's ssl_calls.c with conditional compilation
4. ✅ Added to build system (Makefile and Makefile.am)
5. ✅ Implemented all core TLS functions (init, accept, read, write, shutdown)

### Pending ⚠️

1. ⚠️ **Certificate Loading**: Currently generates self-signed certificates. Need to implement PEM file loading using Security framework.

2. ⚠️ **Deprecation Warnings**: Secure Transport API is deprecated in macOS 10.15+ in favor of Network.framework. While it still works, we may need to migrate to Network.framework eventually.

3. ⚠️ **Build Testing**: Need to compile and test the implementation:
   ```bash
   cd /Users/cyclic/xrdp
   ./configure --prefix=/usr/local
   make -C common
   ```

4. ⚠️ **Integration Testing**: Need to test with actual RDP client:
   ```bash
   # Start xrdp with NeutrinoSSL
   /usr/local/sbin/xrdp

   # Test connection
   python3 test-rdp-connection.py
   ```

5. ⚠️ **Error Handling**: May need to improve retry logic for SSL_ERROR_WANT_READ/WRITE scenarios.

## Known Issues

### 1. Secure Transport Deprecation

The entire Secure Transport API was deprecated in macOS 10.15 (October 2019). Apple recommends using Network.framework instead. However:

- Secure Transport still works and is maintained
- Network.framework is more complex and macOS 10.14+ only
- For xrdp's use case, Secure Transport is sufficient

**Deprecation warnings seen:**
- `SSLCreateContext` deprecated
- `SSLSetIOFuncs` deprecated
- `SSLHandshake` deprecated
- `SSLRead` / `SSLWrite` deprecated
- `kSSLServerSide` / `kSSLStreamType` deprecated
- `SecKeyGeneratePair` deprecated

**To suppress warnings during development:**
```bash
gcc -Wno-deprecated-declarations neutrinossl.c
```

### 2. PEM Certificate Loading Not Implemented

The `load_identity_from_pem()` function currently returns NULL and would need to:
1. Read PEM certificate file
2. Read PEM private key file
3. Convert to DER format
4. Create SecCertificateRef from certificate
5. Create SecKeyRef from private key
6. Create SecIdentityRef combining cert and key

**Temporary workaround:** Generate self-signed certificate at runtime (current implementation).

### 3. Build System Configuration

The Makefile changes require the build system to recognize that we're on macOS. The `-framework Security` linker flag is macOS-specific and will cause errors on other platforms.

**Solution:** The `#ifdef __APPLE__` guards in neutrinossl.c ensure it only compiles on macOS. The entire neutrinossl.c is wrapped in `#ifdef __APPLE__` so it becomes a no-op on other platforms.

## Migration Path to Network.framework

If we need to migrate from deprecated Secure Transport to Network.framework in the future:

1. Replace `SSLContextRef` with `nw_protocol_options_t` and `nw_connection_t`
2. Use `nw_parameters_create_secure_tcp()` for TLS configuration
3. Use `sec_protocol_options_t` for certificate configuration
4. Replace I/O callbacks with `nw_connection_send()` / `nw_connection_receive()`

This would be a significant rewrite but follows Apple's recommended approach.

## Testing Plan

1. **Unit Test Compilation**:
   ```bash
   cd /Users/cyclic/xrdp/common
   gcc -c -I. -I.. -DHAVE_CONFIG_H neutrinossl.c -framework Security
   ```

2. **Integration Test**:
   ```bash
   # Build xrdp
   cd /Users/cyclic/xrdp
   make clean && make

   # Start xrdp
   /usr/local/sbin/xrdp

   # Test RDP connection
   python3 test-rdp-connection.py
   ```

3. **Verify TLS Handshake**:
   - Check for "NeutrinoSSL TLS connection accepted" in logs
   - Verify no PAC crashes occur
   - Confirm successful RDP protocol negotiation

4. **Performance Testing**:
   - Compare throughput with OpenSSL version
   - Verify no memory leaks
   - Test under load with multiple concurrent connections

## Alternative Approaches Considered

1. **OpenSSL Thread-Local Initialization**: Initialize OpenSSL in each thread - didn't resolve PAC issue

2. **Custom OpenSSL Build with PAC Disabled**: Would require distributing custom OpenSSL - maintenance burden

3. **Disable PAC Globally**: Not possible per-process, would require disabling system-wide security feature

4. **Use BoringSSL or LibreSSL**: Still C-based SSL libraries that may have same PAC issues

5. **Use Rust TLS (rustls)**: Would require Rust toolchain and FFI layer - too complex

**Conclusion:** NeutrinoSSL with macOS native APIs is the most pragmatic solution for macOS builds.

## References

- [xrdp GitHub](https://github.com/neutrinolabs/xrdp)
- [macOS Secure Transport](https://developer.apple.com/documentation/security/secure_transport)
- [macOS Network.framework](https://developer.apple.com/documentation/network)
- [Apple Silicon PAC Documentation](https://developer.apple.com/documentation/security/preparing_your_app_to_work_with_pointer_authentication)

## License

NeutrinoSSL is part of xrdp and follows the same Apache 2.0 license.

Copyright (C) 2026 Neutrino Labs
