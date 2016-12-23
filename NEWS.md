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
