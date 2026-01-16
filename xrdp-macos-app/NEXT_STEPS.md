# NeutrinoSSL Implementation - Current Status and Next Steps

## Current Situation

We successfully implemented NeutrinoSSL using Apple's Network.framework and integrated it into xrdp. The code compiles and links properly, but **crashes at runtime** when attempting to establish TLS connections.

## Root Cause

**Architectural Incompatibility:** Network.framework expects to own the entire connection lifecycle (socket creation, connection, TLS handshake). However, xrdp's architecture is:

1. xrdp creates and binds a listening socket
2. xrdp accepts incoming connections (gets a connected socket)
3. xrdp then tries to wrap the existing socket with TLS

This pattern works with OpenSSL and Secure Transport (which allow wrapping existing sockets), but Network.framework doesn't support this model well.

## Crash Evidence

Crash log shows SIGSEGV (null pointer dereference) during TLS operations:
- **Location:** `xrdp_rdp_delete` → `libxrdp_exit`
- **PC:** 0x0 (null function pointer)
- **Thread:** Network.framework connection thread

The TLS handshake times out, then when xrdp tries to clean up, it crashes trying to call Network.framework functions.

## Options Going Forward

### Option 1: Use Secure Transport (RECOMMENDED)
**Status:** Functional but deprecated

Revert NeutrinoSSL to use Secure Transport API instead of Network.framework:
- ✅ Works with xrdp's existing architecture
- ✅ Allows wrapping existing sockets
- ✅ Thread-safe
- ⚠️ Deprecated in macOS 10.15 (but still functional)
- ⚠️ Will generate deprecation warnings

**Implementation:**
- Replace Network.framework code with Secure Transport (SSLCreateContext, SSLHandshake, etc.)
- Suppress deprecation warnings with `-Wno-deprecated-declarations`
- Keep the same neutrinossl.h API

### Option 2: Refactor xrdp for Network.framework
**Status:** Major architectural change required

Modify xrdp to use `nw_listener_t` and let Network.framework handle accept():
- ❌ Requires extensive changes to xrdp core
- ❌ Would break compatibility with xrdp upstream
- ❌ High risk, high effort
- ✅ Uses modern, supported API

### Option 3: Hybrid Approach
**Status:** Experimental

Use OpenSSL on non-Apple-Silicon Macs, NeutrinoSSL (Secure Transport) only on M1/M2/M3:
- ✅ Minimal deprecation warnings (only affects newer Macs)
- ✅ Maintains OpenSSL compatibility where it works
- ⚠️ More complex build configuration

### Option 4: Fix OpenSSL PAC Issues
**Status:** Unknown feasibility

Investigate why OpenSSL has PAC crashes and fix them:
- ❌ OpenSSL internals are complex
- ❌ May require OpenSSL patches
- ❌ PAC issues may be fundamental to OpenSSL's design
- ✅ Would allow using standard OpenSSL

## Recommendation

**Implement Option 1:** Switch NeutrinoSSL to use Secure Transport API.

### Rationale:
1. **It works** - Secure Transport can wrap existing sockets
2. **Minimal changes** - Keep the same API, just change implementation
3. **Still supported** - Deprecated doesn't mean removed; it still functions
4. **Low risk** - Similar to our original plan, just using ST instead of NW.framework
5. **Future-proof path** - If Apple removes Secure Transport, we can revisit Network.framework when xrdp's architecture can support it

### Implementation Plan:
1. Rewrite `common/neutrinossl.c` to use Secure Transport instead of Network.framework
2. Replace `nw_connection_t` with `SSLContextRef`
3. Use `SSLSetIOFuncs` for socket I/O callbacks
4. Keep all the integration in `ssl_calls.c` unchanged
5. Add `-Wno-deprecated-declarations` to suppress warnings
6. Test thoroughly

## Files to Modify

### common/neutrinossl.c
Replace Network.framework code with Secure Transport:

```c
// Instead of:
nw_connection_t connection;
nw_parameters_t parameters;

// Use:
SSLContextRef ssl_ctx;
SecIdentityRef identity;
```

### common/Makefile
Remove Network framework, keep Security framework:
```makefile
libcommon_la_LIBADD = \
  -lpthread \
  $(OPENSSL_LIBS) \
  $(DLOPEN_LIBS) \
  -framework Security  # Remove -framework Network
```

### Build flags
Add deprecation suppression:
```
CFLAGS += -Wno-deprecated-declarations
```

## Testing Plan

1. Rewrite neutrinossl.c with Secure Transport
2. Rebuild xrdp
3. Test TLS handshake with test-rdp-connection.py
4. Test full RDP connection with Microsoft Remote Desktop
5. Verify no PAC crashes on Apple Silicon
6. Run under stress test (multiple concurrent connections)

## Timeline

- **Rewrite neutrinossl.c**: 1-2 hours
- **Testing**: 1 hour
- **Bug fixes**: 1-2 hours
- **Total**: Half day of work

## Long-term Strategy

When xrdp's architecture is refactored to support listener-based accept (if ever):
1. Revisit Network.framework implementation
2. Use `nw_listener_create()` and `nw_listener_set_new_connection_handler()`
3. Remove Secure Transport entirely
4. Benefit from modern, fully-supported API

Until then, Secure Transport is the pragmatic solution that works today.

## Current Code Status

### What Works ✅
- NeutrinoSSL compiles successfully
- Integrates into xrdp build system
- Links with Network and Security frameworks
- xrdp starts and listens on port 3389

### What Doesn't Work ❌
- TLS handshake times out
- Crashes when trying to cleanup failed connection
- Network.framework connection never completes

### Why
Network.framework's `nw_connection_create()` expects to create its own socket, but we're trying to use an already-accepted socket from xrdp's listener.

---

**Decision Required:** Proceed with Option 1 (Secure Transport) or explore other options?
