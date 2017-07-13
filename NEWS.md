# Release notes for xrdp v0.9.3 (2017/07/15)

## New features
  * Log user-friendly messages when certificate/privkey is inaccessible

## Bugfixes
  * Now sesman sets mandatory LOGNAME environment variable #725
  * Now sesman ensures socket directory present #801
  * Exit with failure status if port already in use #644
  * Eliminate some hard coded paths
  * Fix a glitch with IPv4 struct initialization #803
  * Fix some keyboard layout integration (UK, Spanish)
  * Fix handle OS when IPv6 disabled #714
  * Fix issues around systemd session #778
  * Fix protocol error when 32 bit color and non RemoteFX session #737 #804
  * Fix sesadmin shows error when no sessions #797
  * Fix TLS spins 100% CPU #728
  * Fix Xvnc backend disconnects when some data copied to clipboard #755
  * Pick up the first section if given section(domain) doesn't match anything #750

## Other changes
  * Change xrdp-chansrv log path to include display number
  * Optimize startwm.sh for SUSE
  * Several cleanups and optimizations

## Known issues
  * Windows 10 (1703) shows black blank screen in RemoteFX mode

-----------------------

# Release notes for xrdp v0.9.2 (2017/03/30)
## New features
  * RemoteFX codec support is now enabled by default.
  * Bitmap updates support is now enabled by default.
  * TLS ciphers suites and version is now logged.
  * Connected computer name is now logged.
  * Switched to Xorg (xorgxrdp) as the default backend now.
  * Miscellaneous RemoteFX codec mode improvements.
  * Socket directory is configurable at the compile time.

## Bugfixes
  * Parallels client for MacOS / iOS can now connect (audio redirection must be disabled on client or xrdp server though).
  * MS RDP client for iOS can now connect using TLS security layer.
  * MS RDP client for Android can now connect to xrdp.
  * Large resolutions (4K) can be used with RemoteFX graphics.
  * Multiple RemoteApps can be opened throguh NeutrinoRDP proxy.
  * tls_ciphers in xrdp.ini is not limited to 63 chars anymore, it's variable-length.
  * Fixed an issue where tls_ciphers were ignored and rdp security layer could be used instead.
  * Kill disconnected sessions feature is working with Xorg (xorgxrdp) backend.
  * Miscellaneous code cleanup and memory issues fixes.

-----------------------

# Release notes for xrdp v0.9.1 (2016/12/21)
## New features
  * New xorgxrdp backend using existing Xorg with additional modules
  * Improvements to X11rdp backend
  * Support for IPv6 (disabled by default)
  * Initial support for RemoteFX Codec (disabled by default)
  * Support for TLS security layer (preferred over RDP layer if supported by the client)
  * Support for disabling deprecated SSLv3 protocol and for selecting custom cipher suites in xrdp.ini
  * Support for bidirectional fastpath (enabled in both directions by default)
  * Support clients that don't support drawing orders, such as MS RDP client for Android, ChromeRDP (disabled by default)
  * More configurable login screen
  * Support for new virtual channels:
      * rdpdr: device redirection
      * rdpsnd: audio output
      * cliprdr: clipboard
      * xrdpvr: xrdp video redirection channel (can be used along with NeutrinoRDP client)
  * Support for disabling virtual channels globally or by session type
  * Allow to specify the path for backends (Xorg, X11rdp, Xvnc)
  * Added files for systemd support
  * Multi-monitor support
  * xrdp-chansrv stroes logs in `${XDG_DATA_HOME}/xrdp` now

## Security fixes
  * User's password could be recovered from the Xvnc password file
  * X11 authentication was not used
