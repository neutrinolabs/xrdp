# Release notes for xrdp v0.9.19 (2022/03/17)

## General announcements
* Running xrdp and xrdp-sesman on separate hosts is still supported by this release, but is now deprecated. This is not secure. A future release will replace the TCP socket used between these processes with a Unix Domain Socket, and then cross-host running will not be possible.

## New features
* Both inbound and outbound clipboards can now be restricted for text, files or images [Sponsored by @CyberTrust @clear-code and @kenhys] (#2087)

## Bug fixes
* [CVE-2022-23613](https://www.cve.org/CVERecord?id=CVE-2022-23613): Privilege escalation on xrdp-sesman (This fix is also in the out-of-band v0.9.18.1 release)
* The versions of imlib2 used on RHEL 7 and 8 are now detected correctly (#2118)
* Some situations where zombie processes could exist have been resolved (#2146, #2151, #2168)
* Some null-pointer exceptions which can happen in the logging module have been addressed (#2149)
* Some minor logging errors have been corrected (#2152)
* The signal handling in sesman has been reworked to prevent race conditions when a child exits. This has also made it possible to reliably reload the sesman configuration with SIGHUP (#1729, #2168)

## Internal changes
* Versions 0.13 and later of checklib can undefine the pre-processor symbol `HAVE_STDINT_H`. The xrdp tests now build successfully against these versions (#2124)
* OpenSSL packaging changes (#2130):-
   - The OpenSSL 3 EVP interface is now fully supported
   - When building against OpenSSL 3, an internal implementation of the RC4 cipher is used instead of the implementation from the OpenSSL legacy provider
   - The wrapping of the OpenSSL library has been improved which should make it simpler to provide an alternative cryptographic provider in the future, if required
   - The logging of TLS/non-TLS security negotiation has been improved
* cppcheck version used for CI bumped to 2.7 (#2140)
* The `s_check()` macro which is easily mis-used has been removed (#2144)
* Status values for the DRDYNVC channel are now available in `libxrdp/xrdp_channel.h`

## Changes for packagers or developers
* On OpenSSL 3 systems, there is now no need to build with the `-Wno-error=deprecated-declarations` flag

## Known issues

* On-the-fly resolution change requires the Microsoft Store version of Remote Desktop client but sometimes crashes on connect (#1869)
* xrdp's login dialog is not relocated at the center of the new resolution after on-the-fly resolution change happens (#1867)

-----------------------

# Release notes for xrdp v0.9.18.1 (2022/02/08)

This is a security fix release that includes fixes for the following privilege escalation vulnerability.

* [CVE-2022-23613: Privilege escalation on xrdp-sesman](https://www.cve.org/CVERecord?id=CVE-2022-23613)

Users who uses xrdp v0.9.17 or v0.9.18 are recommended to update to this version.

## Special thanks

Thanks to [Gilad Kleinman](https://github.com/giladkl) reporting the vulnerability and reviewing fix.

-----------------------

# Release notes for xrdp v0.9.18 (2022/01/10)

## General announcements
* Running xrdp and xrdp-sesman on separate hosts is still supported by this release, but is now deprecated. This is not secure. A future release will replace the TCP socket used between these processes with a Unix Domain Socket, and then cross-host running will not be possible.
* Special thanks for @trishume for contributing code to the RFX codec

## New features
* Backgrounds and logos on the login screen can now be zoomed and scaled (#1962)
* Small change for Alpine Linux support (#2005)
* loongarch support (#2057)
* Improved Fail2ban support (#1976)

## Bug fixes
* Logging is improved for security protocol level decisions (#1974, #1975)
* An unnecessary log error message which is always generated when running neutrinordp has been removed (#2016)
* An incorrect development log message has been fixed (#2074)
* Some informational and error messages written to the console on stdout have been removed or replaced with log messages (#2078 #2080)
* Failure to attach to the memory area shared with xorgxrdp is now logged (#2065)
* A regression in the VNC module logging which might cause a connection to drop out has been identified and fixed (#1989)
* Remote drive redirection now works if printer redirection is also requested by the client (#327)
* Some file names could not be copied from the client to the server over the clipboard. This is now fixed (#1992, #1995)
* A config value has been added which allows copy-pasting of files to work with Nautilus for GNOME 3 versions >= 3.29.92 (#1994, #1996)
* Clipboard now works properly when files can't be read (#1997 #2001)
* (xorgxrdp v0.2.18) The screen is fully refreshed after initialising shared memory which should fix black screen problems like #1964
* An incorrect initialisation reported by @qarmin has been fixed (#1909)
* Some minor memory leaks have been fixed (#2014 #2028)
* A hard hang in chansrv when copying files from the remote system has been addressed (#2032)
* Users can now capitalise username and password on the login screen if required (#2061)
* Some failed size checks in the fastpath code with `--enable-devel-streamcheck` have been addressed (#2066,#2070)
* Log level for clipboard restriction has been promoted from DEVEL DEBUG to INFO  (#2088)
* A buffer overflow in the RFX codec associated with large screens has been fixed (#2087)

## Internal changes
* Some 64-bit packages are removed during the 32-bit CI build process in an attempt to make this more robust (#1985)
* Minor improvements to error checking and logging for file copy-paste (#1996)
* Now uses cppcheck 2.6 for CI builds (#2008)
* Generated systemd unit files now ignored by git (#2006)
* More internal tests (#2015)
* Some unnecessary files have been removed from the distribution (#2030)
* The `which` command in shell scripts has been replaced with `command -v` (#2067)
* Additional unit tests added for `g_file_get_size()` (#1988)
* A compiler warning with -O3 on gcc 11.1 has been addressed (#2105)
* An unused declaration for xrdp_wm_drdynvc_up has been removed (#2098)
* The SCP V0 code has been unified, which will make it easier to update and replace (#2011)
* Monitor processing unit tests for existing xrdp_sec function have been added (#1932)
* The librfxcodec has been updated as part of #2087, and also to add stack frames to assemble code to assist debugging

## Changes for packagers or developers
* The `--with-imlib2` option has been added. If xrdp is built with imlib2, the login screen supports more image formats for the background and logo, and better quality zooming and scaling (#1962)

## Known issues

* On-the-fly resolution change requires the Microsoft Store version of Remote Desktop client but sometimes crashes on connect (#1869)
* xrdp's login dialog is not relocated at the center of the new resolution after on-the-fly resolution change happens (#1867)

-----------------------

# Release notes for xrdp v0.9.17 (2021/08/31)

## General announcements
* Running xrdp and xrdp-sesman on separate hosts is still supported by this release, but is now deprecated. This is not secure. A future release will replace the TCP socket used between these processes with a Unix Domain Socket, and then cross-host running will not be possible.

## New features
* The IP address, port, and user name of NeutrinoRDP Proxy connection are logged in xrdp.log - these connections may not have a sesman log to use (#1873)
* The performance settings for NeutrinoRDP can be now configured (#1903)
* Support for Alpine Linux in startwm.sh (#1965)
* clipboard: log file transfer for the purpose of audit (#1954)
* Client's Keyboard layout now can be overridden by xrdp configuration for debugging purposes (#1952)

## Bug fixes
* PAM_USER environment variable is not set when using pam_exec module (#1882)
* Allow common channel settings to be overridden for modules as well as chansrv (#1899)
* The text only-copy/paste interface for the VNC module (used only when chansrv is not active) has been improved (#1900)
* The unsupported `tcutils` utility has been removed (#1943)
* The quality of TLS logging has been improved (#1926)
* Keyboard information is now passed correctly through NeuutrinoRDP, and can be overridden if required (#1934)
* A message is now logged in the sesman log for unsuccessful login attempts detailing the user used (#1947)


## Internal changes
* astyle formatting is now checked during CI builds (#1879)
* Generalise development build options, and add --enable-devel-streamcheck (#1887)
* Now uses cppcheck 2.5 for CI builds (#1938)
* The SCP protocol is now using a standard `struct trans` for messaging rather than its own thing (#1925)

## Changes for packagers or developers
* The `--enable-xrdpdebug` developer option has been replaced with finer-grained `--enable-devel-*` options. Consequently, specifying `--enable-xrdpdebug` is now an error (#1913)

## Known issues

* On-the-fly resolution change requires the Microsoft Store version of Remote Desktop client but sometimes crashes on connect (#1869)
* xrdp's login dialog is not relocated at the center of the new resolution after on-the-fly resolution change happens (#1867)

-----------------------

# Release notes for xrdp v0.9.16 (2021/04/30)

## New features
* On-the-fly resolution change now supported for Xvnc and Xorg (#448, #1820) - thanks to @Nexarian for this significant first contribution. See the following YouTube video for a demo.
    * [Windows] https://youtu.be/cZ0ebieZHeA
    * [Mac] https://youtu.be/6kfAkyLUgFY
* xrdp can now use key algorithms other than RSA for TLS (#1776)
* Do not spit on the console 2nd stage (inspired by Debian) #1762
* Unified and improved logging (#1742, #1767, #1802, #1806, #1807, #1826, #1843) - thanks to @aquesnel for this detailed work.
* Other logging level fixes (#1864)
* chansrv can now work on `DISPLAY=:0` so it can be used with x11vnc/Vino/etc sessions (#1849)

## Bug fixes
* Fix some regressions in sesman auth modules (#1769)
* Minor manpage fixes (#1787)
* Fix TS_PLAY_SOUND_PDU_DATA to set the correct frequency and duration (#1793)
* Fix password leakage to logs in NeutrinoRDP module (#1872) - thanks to @TOMATO-ONE for reporting.

## Internal changes
* cppcheck version for CI bumped to 2.4 (#1771, #1836)
* FreeBSD version for CI bumped to 12-2 (#1804)
* Support for check unit test framework added (#1843, #1860)
* FreeBSD FUSE module now compiles under CI but needs additional work (#1856)
* Compilation support added for additional Debian platforms (#1818)
* Refactoring:-
   * Confusing preprocessor macro USE_NOPAM replaced with USE_PAM (#1800)
   * Window manager states in xrdp executable now use symbolic constants instead of numbers (#1803)
* Documentation improvements
   * KRDC added to client list (#1817)
   * Platform support tier added (#1822)
   * README file revised (#1863)
* Don't install test+development executables by default (#1858)

## Changes for packagers
These changes are likely to impact operating system package builders and those building xrdp from source.
* (#1843, #1860) This release introduces an additional optional compile-time dependency on the `check` unit test framework. The dependency is recommended when packaging for compile-time tests.
* (#1858) The executables `memtest` and `tcp_proxy` are no longer copied to the sbin directory on a package install.

## Known issues

* On-the-fly resolution change requires the Microsoft Store version of Remote Desktop client but sometimes crashes on connect (#1869)
* xrdp's login dialog is not relocated at the center of the new resolution after on-the-fly resolution change happens (#1867)

-----------------------

# Release notes for xrdp v0.9.15 (2020/12/28)

## New features
* Allow token sign in without autologon for SSO (#1667 #1668)
* Norwegian keyboard support (#1675)
* Improved config support for chansrv (#1635)
* Unified chansrv, sesman and libxrdp logging (#1633 #1708 #1738) - thanks to @aquesnel
* Support SUSE move to /usr/etc (#1702)
* Parameters may now be specified for user-specified shell (#1270 #1695)
* xrdp executables now allow alternative config files to be specified with -c (#1588 #1650 #1651)
* sesrun improvements (#1741)
* Drive redirection location can now be specified (#1048)
* Now compiles on RISC-V (#1761)

## Bug fixes
* Additional buffer overflow checks (#1662)
* FUSE support now builds on 32-bit platforms (#1682)
* genkeymap array size conflict fixed (#1691)
* Buffering issue with neutrinordp over a slow link fixed (#1608 1634)
* Various documentation fixes (#1704 #1741 #1755 #1759)
* Prevent PAM info message from causing authentication failure (#1727)
* Cosmetic fixes for minor issues (#1751 #1755 #1749)
* Try harder to clean up socket files on session exit (#1740 #1756)
* xrdp-chansrv become defunct in docker while file copy (#1658)

## Internal changes
* Compilation warnings with newer compilers (#1659 #1680)
* Continuation Integration checks on 32-bit platforms now include FUSE support (#1682)
* Continuation Integration builds now default to the Ubuntu Focal platform (#1666)
* FUSE type tidy-ups (#1686)
* Switch from Travis CI to GitHub Actions (#1728 #1732)
* Easier to set up console logging for utilities (#1711)

-----------------------

# Release notes for xrdp v0.9.14 (2020/08/31)

## New features
* VNC multi-monitor support if you are using a suitable Xvnc server #1343
* VNC sessions now resize by default on reconnection if you are using a suitable Xvnc server #1343
* Support Slackware for PAM #1558 #1560
* Support Programmer Dvorak Keyboard #1663

**[HEADS UP]** The VNC changes are significant. They described in more detail on the following wiki page.
* [Xvnc backend : Multi monitor and resize support](https://github.com/neutrinolabs/xrdp/wiki/Xvnc-backend-:-Multi-monitor-and-resize-support)

## Bug fixes
* Fix odd shift key behavior (workaround) #397 #1522
* Fix Xorg path in the document for Arch Linux #1448 #1529
* Fix Xorg path in the document for CentOS 8 #1646 #1647
* Fix internal username/password buffer is smaller than RDP protocol specification #1648 #1653
* Fix possible memory out-of-bounds accesses #1549
* Fix memory allocation overflow #1557
* Prevent chansrv input channels being scanned during a server reset #1595
* Ignore TS_MULTIFRAGMENTUPDATE_CAPABILITYSET from client if fp disabled #1593
* Minor manpage fixes #1611

## Other changes
* CI error fixes 
* Introduce cppcheck

## Known issues
* FreeRDP 2.0.0-rc4 or later might not able to connect to xrdp due to
  xrdp's bad-mannered behaviour, add `+glyph-cache` option to FreeRDP to connect #1266
* Audio redirection by MP3 codec doesn't sound with some client, use AAC instead #965

# Release notes for xrdp v0.9.13.1 (2020/06/30)

This is a security fix release that includes fixes for the following local buffer overflow vulnerability.

* [CVE-2020-4044: Local users can perform a buffer overflow attack against the xrdp-sesman service and then impersonate it](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2020-4044)

This update is recommended for all xrdp users.

## Special thanks

Thanks to [Ashley Newson](https://github.com/ashleynewson) reporting the vulnerability and reviewing fix.

-----------------------

# Release notes for xrdp v0.9.13 (2020/03/11)

This release is an intermediate bugfix release. The previous version v0.9.12 has some regressions on drive redirection.

## Bug fixes (drive redirection related)
* Fix chansrv crashes with segmentation fault (regression in #1449) #1487
* Drive redirection now supports Guacamole client #1505 #1507
* Prevent a coredump in the event of a corrupted file system #1507
* Resolve double-free in `chansrv_fuse` #1469

## Bug fixes (other)
* Fix the issue `xrdp --version | less` will show empty output #1471 #1472
* Fix some warnings found by cppcheck #1479 #1481 #1484 #1485

## Other changes
* Add FreeBSD CI test #1466
* Move Microsoft-defined constants into separate includes #1470
* Perform cppcheck during CI test #1493
* Support mousex button 8/9 #1478

## Known issues
* FreeRDP 2.0.0-rc4 or later might not able to connect to xrdp due to
  xrdp's bad-mannered behaviour, add `+glyph-cache` option to FreeRDP to connect #1266
* Audio redirection by MP3 codec doesn't sound with some client, use AAC instead #965

-----------------------

# Release notes for xrdp v0.9.12 (2019/12/28)

## Bug fixes
* Fix "The log reference is NULL" error when sesman startup #1425
* Fix behavior when shmem_id changes #1439
* Make vsock config accept -1 for cid and port #1441
* Cleanup refresh rect and check stream bounds #1437
* Significant improvements in drive redirection #1449
* Fix build on macOS Catalina #1462

## Other changes
* Proprietary microphone redirection via rdpsnd is now default off
  RDP compatible microphone redirection is on instead #1427
* Skip connecting to chansrv when no channels enabled #1393
* Add openSUSE's pam rules #1442
* Do not terminate xrdp daemon when caught SIGHUP #1319

## Known issues
* FreeRDP 2.0.0-rc4 or later might not able to connect to xrdp due to
  xrdp's bad-mannered behaviour, add `+glyph-cache` option to FreeRDP to connect #1266
* Audio redirection by MP3 codec doesn't sound with some client, use AAC instead #965

# Release notes for xrdp v0.9.11 (2019/08/19)

## New features
* Suppress output (do not draw screen when client window is minimized) #1330
* Audio input (microphone) redirection compatible with [MS-RDPEAI](https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-rdpeai/d04ffa42-5a0f-4f80-abb1-cc26f71c9452) #1369
* Now xrdp can listen on more than one port #1124 #1366

## Bug fixes
* Fix the issue audio redirection sometimes sounds with long delay  #1363
* Check term event for more responsive shutdown #1372

## Known issues
* FreeRDP 2.0.0-rc4 or later might not able to connect to xrdp due to
  xrdp's bad-mannered behaviour, add `+glyph-cache` option to FreeRDP to connect #1266
* Audio redirection by MP3 codec doesn't sound with some client, use AAC instead #965

-----------------------

# Release notes for xrdp v0.9.11 (2019/08/19)

## New features
* Suppress output (do not draw screen when client window is minimized) #1330
* Audio input (microphone) redirection compatible with [MS-RDPEAI](https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-rdpeai/d04ffa42-5a0f-4f80-abb1-cc26f71c9452) #1369
* Now xrdp can listen on more than one port #1124 #1366

## Bug fixes
* Fix the issue audio redirection sometimes sounds with long delay  #1363
* Check term event for more responsive shutdown #1372

## Known issues
* FreeRDP 2.0.0-rc4 or later might not able to connect to xrdp due to
  xrdp's bad-mannered behaviour, add `+glyph-cache` option to FreeRDP to connect #1266
* Audio redirection by MP3 codec doesn't sound with some client, use AAC instead #965

-----------------------

# Release notes for xrdp v0.9.10 (2019/04/18)

## Special thanks
Thank you for matt335672 contributing to lots of improvements in drive redirection!

## New features
* Restrict outbound (server->client) clipboard transfer, configured in `sesman.ini` #1298

## Bug fixes
* Fix the issue libscp v1 not setting width but height twice #1293
* Fix the issue reconnecting to session causes duplicate drive entries in fuse fs #1299
* Fix default_wm and reconnect_sh refer wrong path after sesman caught SIGUP #1315 #1331
* Shutdown xrdp more responsively #1325
* Improve remote file lookup in drive redirection #996 #1327
* Overwriting & appending to existing files is are now supported #1327

## Other changes
* Add Danish Keyboard #1290
* Put xrdp- prefix to some executables appear in man page #1313
* Replace some URLs from SF.net to xrdp.org #1313

## Known issues
* FreeRDP 2.0.0-rc4 or later might not able to connect to xrdp due to
  xrdp's bad-mannered behaviour, add `+glyph-cache` option to FreeRDP to connect #1266
* Audio redirection by MP3 codec doesn't sound with some client, use AAC instead #965

-----------------------

# Release notes for xrdp v0.9.9 (2018/12/25)

## Release cycle
From the next release, release cycle will be changed from quarterly to every
4 months. xrdp will be released in April, August, December.

## New features
* Disconnection by idle timeout (requires xorgxrdp v0.2.9 or later) #1227
* Glyph cache v2 (fixes no font issue on iOS/macOS/Android client) #367 #1235

## Bug fixes
* Fix xrdp-chansrv crashes caused in drive redirection #1202 #1225
* Fix build with FDK AAC v2 #1257
* Do not enable RemoteApp if the INFO_RAIL flag is not set (RDP-RDP proxy) #1253

## Other changes
* Add Spanish Latin Amarican keyboard #1237 #1240 #1244
* Dynamic channel improvements #1222 #1224
* Remove some deprecated sesman session types #1232
* Refactoring and cleanups

## Known issues
* FreeRDP 2.0.0-rc4 or later might not able to connect to xrdp due to
  xrdp's bad-mannered behaviour, add `+glyph-cache` option to FreeRDP to connect #1266
* Audio redirection by MP3 codec doesn't sound with some client, use AAC instead #965

-----------------------

# Release notes for xrdp v0.9.8 (2018/09/25)

## Deprecation notice
We removed TLSv1 and TLSv1.1 from the default config. The current default is TLSv1.2
and TLSv1.3. Users can whenever re-enable these early TLS versions by editing xrdp.
To use TLSv1.3, OpenSSL or LibreSSL must support TLSv1.3. You can know the OpenSSL
or LibreSSL version by `xrdp --version` command that compiled with xrdp.

## Other topics

Pulseaudio modules has been removed from xrdp source tree since it is actually
independent and not part of xrdp. The repository has been moved to:
https://github.com/neutrinolabs/pulseaudio-module-xrdp

If you want to use audio redirection, make sure install the module separately.

## New features
* Add TLSv1.3 support #1193

## Bug fixes
* Ensure unmount redirected drive on fatal X error #1140

## Other changes
* Show more helpful message if xrdp-dis failed #1206
* Pass pulse socket name via environment variable #1198
* Fix xrdp's log path in man page #1168

# Release notes for xrdp v0.9.7 (2018/06/29)

## Deprecation notice
x11rdp has been removed from xrdp reposiory and stored in the separate repository.
Checkout [x11rdp repository](https://github.com/neutrionlabs/x11rdp) if you still need x11rdp.
In most cases, [xorgxrdp](https://github.com/neutrinolabs/xorgxrdp) can replace x11rdp.

## Bug fixes
* Fix endianness detection on ppc64el #1082
* Fix a bug xrdp file copy slow #1112 #1132
* Copy the PAM session environment for the reconnect script #1120
* Accept fullpath for DefaultWindowManager, ReconnectScript #1147

## Other changes
* Add PAM support for Arch Linux #1078
* Show OpenSSL version to '--version' CLI option #1096
* Separate x11rdp from xrdp repository #1104
* Support sesrun start xorgxrdp sessions #1108
* Show configure summary when configure is done #1126 #1134 #1137
* Less spit on the console when sesman starts #1142
* Fix memory leaks #1146
* Separate rc script for FreeBSD into xrdp and xrdp-sesman #1153
* Improve documents and helps

## Known issues
* Audio redirection by MP3 codec doesn't sound with some client, use AAC instead #965

-----------------------

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
