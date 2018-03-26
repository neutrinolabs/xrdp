# Release notes for xrdp v0.9.6 (2018/03/26)

## Compatibility notice
Exclamation mark (`!`) has been removed from comment out symbol of config files.
Use number sign (`#`) or semicolon (`;`) instead. As a result of this change, now
you can use exclamation mark as config value such as in `tls_ciphers`.

```
tls_ciphers=HIGH:!aNULL:!eNULL:!EXPORT:!RC4
```

See also: #1033

## macOS supports
Please note that xrdp still doesn't support macOS officially so far.
However, a volunteer is working on macOS compatibility.

* Generate dylibs for macOS #1015
* Add PAM support for macOS #1021

## Bug fixes
* Make listen check before daemon fork #988
* Fix xrdp sometimes become zombie processes #1000
* Include hostname in sesman password file name #1006 #1007 #1076
* Fix default startwm.sh to use bash explicitly #1009 #1049
* Fix the issue FreeBSD doesn't acknowledge terminated sessions #1016 #1030

## Other changes
* Add Swiss French keyboard #1053
* Improve perfect forward secrecy, explicitly enable ECDHE/DHE #1024 #1052 #1063
* Lots of leak fixes, cleanups and refactoring

## Known issues
* Audio redirection by MP3 codec doesn't sound with some client, use AAC instead #965

-----------------------

# Release notes for xrdp v0.9.5 (2017/12/27)

## Security fixes
* Fix local denial of service [CVE-2017-16927](http://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2017-16927) #958 #979

## New features
* Add a new log level TRACE more verbose than DEBUG #835 #944
* SSH agent forwarding via RDP #867 #868 FreeRDP/FreeRDP#4122
* Support horizontal wheel properly #928

## Bug fixes

* Avoid use of hard-coded sesman port #895
* Workaround for corrupted display with Windows Server 2008 using NeutrinoRDP #869
* Fix glitch in audio redirection by AAC #910 #936
* Implement vsock support #930 #935 #948
* Avoid 100% CPU usage on SSL accept #956

## Other changes
* Add US Dvorak keyboard #929
* Suppress some misleading logs #964
* Add Finnish keyboard #972
* Add more user-friendlier description about Xorg config #974
* Renew pulseaudio document #984 #985
* Lots of cleanups and refactoring

## Known issues
* Audio redirection by MP3 codec doesn't sound with some client, use AAC instead #965

-----------------------

# Release notes for xrdp v0.9.4 (2017/09/28)

## New features
  * Accept prefill credentials in base64 form #153 #811
  * Indroduce AAC encoder to audio redirection (requires Windows 10 client)

## Bugfixes
  * Fix ocasional SEGV in drive redirection #838
  * Fix client's IP addresses in xrdp-sesman.log are always logged as `0.0.0.0` #878 #882
  * Fix `ls_background_image` didn't accept full path #776 #853
  * Fix misuse of hidelogwindow #414 #876
  * Fix WTSVirtualChannelWrite return code #859
  * Fix no longer needed socket files remained in the socket dir #812 #831
  * Make creating socket path a bit more robust #823

## Other changes
  * Add Belgian keyboard #858
  * Add a PAM file for FreeBSD #824
  * Several refactorings and cosmetic changes

## Known issues
  * Windows 10 (1703) shows black blank screen in RemoteFX mode
   * This issue is already fixed at Insider Preview build 16273

-----------------------

# Release notes for xrdp v0.9.3.1 (2017/08/16)

This release fixes a trivial packaging issue #848 occurred in v0.9.3.  The issue only affects systemd systems.  This release is principally for distro packagers or users who compile & install xrdp from source.

Users who running xrdp on these systems don't need to upgrade from v0.9.3 to v0.9.3.1.

* Linux systems without systemd
* non-Linux systems such as BSD operating systems

-----------------------

# Release notes for xrdp v0.9.3 (2017/07/15)

## New features
  * Log user-friendly messages when certificate/privkey is inaccessible

## Bugfixes
  * Now sesman sets mandatory LOGNAME environment variable #725
  * Now sesman ensures socket directory present #801
  * Exit with failure status if port already in use #644
  * Eliminate some hard coded paths
  * Fix glitches with IPv4 struct initialization #803
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
