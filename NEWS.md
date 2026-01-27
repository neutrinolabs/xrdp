# Release notes for xrdp v0.10.5 (2026/01/27)

## General announcements

If you like xrdp, please consider sponsoring or donating to the project. We accept financial contributions through [Open Collective](https://opencollective.com/xrdp-project), and direct donations to individual developers via GitHub Sponsors are also welcome.

* [V0.10.3] Experimental support for utmp/wtmp file is provided in this release. If you use this, be aware that these files are only updated when an xrdp session is created or destroyed. Disconnections and reconnections to the same session are not tracked. In particular:
  * the FROM address for a client (as shown by the `w` command) reflects the IP address of the client at the time of creation, and not the address of the currently connected client.
  * Sessions started by the `xrdp-sesrun` command do not have a FROM address.
* The use_vsock parameter in xrdp.ini is deprecated. Use 'port=vsock://' instead.

## Security fixes
* [CVE-2025-68670: Improper bounds checking of domain string length leads to Stack-based Buffer Overflow](https://nvd.nist.gov/vuln/detail/CVE-2025-68670)

## New features
- It is now possible to start the xrdp daemon entirely unprivileged from the service manager (#3599 #3603). If you do this certain restrictions will apply. See https://github.com/neutrinolabs/xrdp/wiki/Running-the-xrdp-process-as-non-root for details.
- TLS pre-master secrets can now be recorded for packet captures (#3617)
- Add a `FuseRootReportMaxFree` to work around 'no free space' issues with some file managers (#3639)
- Alternate shell names can now be passed to startwm.sh in an environment variable for more system management control (#3624 #3651)
- Updated Xorg paths in sesman.ini to include more recent distros (#3663)
- Add Slovenian keyboard (#3668 #3670)
- xrdpapi: Add a way to monitor connect/disconnect events (#3693)


## Bug fixes
- Allow an empty X11 UTF8_STRING to be pasted to the clipboard (#3580 #3582)
- Fix a regression introduced in v0.10.x, where it became impossible to connect to a VNC server which did not support the ExtendedDesktopSize encoding (#3540 #3584)
- Fix a regression introduced in v0.10.x related to PAM groups handling (#3594)
- Inconsistencies with [MS-RDPBCGR] have been addressed (#3608)
- A reference to uninitialised data within the verify_user_pam_userpass.c module has been fixed (#3638)
- Prevent some possible crashes when the RFX encoder is resized (#3590 #3644)
- Fixes a regression introduced by GFX development which prevented the JPEG encoder from working correctly (#3649)
- Fixes a regression introduced by #2974 which resulted in the xrdp PID file being deleted unexpectedly (#3650)
- Do not overwrite a VNC port set by the user when not using sesman (#3674)
- Fix regression from 0.9.x when freerdp client uses /workarea (#3618 #3676)
- Fixes a crash where a resize is attempted with drdynvc disabled (#3672 #3680)
- getgrouplist() now compiles on MacOS (#3575)
- Various Coverity warnings have been addressed (#3656)
- Documentation improvements (#3665)

## Internal changes
- An unnecessary include of sys/signal.h causing a compile warning on MUSL-C has been removed (#3679)

## Changes for users
None

## Changes for packagers or developers
* (from v0.10.3) The `--enable-utmp` needs to be added to enable UTMP support.
* (from v0.10.3) The config file subdirectory (`xrdp` part of `/etc/xrdp`) can now be configured (#3369)
* (from v0.10.3) Packagers using TigerVNC to provide the Xvnc backend may wish to configure the 'Xvnc over UDS' session type as a default by using a `code=1` line in xrdp.ini. Instructions are provided in the released xrdp.ini file.

-----------------------

# Release notes for xrdp v0.10.4.1 (2025/07/07)

## General announcements

This is a bug-fix release for xrdp v0.10.4

If you like xrdp, please consider sponsoring or donating to the project. We accept financial contributions through [Open Collective](https://opencollective.com/xrdp-project), and direct donations to individual developers via GitHub Sponsors are also welcome.

* [V0.10.3] Experimental support for utmp/wtmp file is provided in this release. If you use this, be aware that these files are only updated when an xrdp session is created or destroyed. Disconnections and reconnections to the same session are not tracked. In particular:
  * the FROM address for a client (as shown by the `w` command) reflects the IP address of the client at the time of creation, and not the address of the currently connected client.
  * Sessions started by the `xrdp-sesrun` command do not have a FROM address.
* The use_vsock parameter in xrdp.ini is deprecated. Use 'port=vsock://' instead.

## Security fixes
None

## New features
None

## Bug fixes
* A regression which prevented xorgxrdp v0.10.4 working with this release has been addressed (#3561)

## Internal changes
None

## Changes for users
None

## Changes for packagers or developers
* (from v0.10.3) The `--enable-utmp` needs to be added to enable UTMP support.
* (from v0.10.3) The config file subdirectory (`xrdp` part of `/etc/xrdp`) can now be configured (#3369)
* (from v0.10.3) Packagers using TigerVNC to provide the Xvnc backend may wish to configure the 'Xvnc over UDS' session type as a default by using a `code=1` line in xrdp.ini. Instructions are provided in the released xrdp.ini file.

-----------------------

# Release notes for xrdp v0.10.4 (2025/07/02)

## General announcements

If you like xrdp, please consider sponsoring or donating to the project. We accept financial contributions through [Open Collective](https://opencollective.com/xrdp-project), and direct donations to individual developers via GitHub Sponsors are also welcome.

* [V0.10.3] Experimental support for utmp/wtmp file is provided in this release. If you use this, be aware that these files are only updated when an xrdp session is created or destroyed. Disconnections and reconnections to the same session are not tracked. In particular:
  * the FROM address for a client (as shown by the `w` command) reflects the IP address of the client at the time of creation, and not the address of the currently connected client.
  * Sessions started by the `xrdp-sesrun` command do not have a FROM address.
* The use_vsock parameter in xrdp.ini is deprecated. Use 'port=vsock://' instead.

## Security fixes
None

## New features
* When running as a Hyper-V VM, additional security features can be provided by setting the vmconnect parameter in xrdp.ini. Thanks to @gpotter2 for this great feature (#3524)
* Add Latvian keyboards (#3511, #3519)

## Bug fixes
* systemd detection has been improved on Debian-based systems (#3497, #3502)
* xrdp sessions fail with Quest/OneIdentity Safeguard for Privileged Sessions (#3498, #3507)
* A race condition at chansrv startup which can result in chansrv not being killed has been addressed (#3482)
* Various Coverity warnings have been addressed (#3508)
* A possible double-free on chansrv exit has been addressed (#3546)

## Internal changes
* The embedded TOML-C99 library is updated to the latest version (#3530)

## Changes for users
None

## Changes for packagers or developers
* (from v0.10.3) The `--enable-utmp` needs to be added to enable UTMP support.
* (from v0.10.3) The config file subdirectory (`xrdp` part of `/etc/xrdp`) can now be configured (#3369)
* (from v0.10.3) Packagers using TigerVNC to provide the Xvnc backend may wish to configure the 'Xvnc over UDS' session type as a default by using a `code=1` line in xrdp.ini. Instructions are provided in the released xrdp.ini file.

-----------------------

# Release notes for xrdp v0.10.3 (2025/03/30)

## General announcements

If you like xrdp, please consider sponsoring or donating to the project. We accept financial contributions through [Open Collective](https://opencollective.com/xrdp-project), and direct donations to individual developers via GitHub Sponsors are also welcome.

* Experimental support for utmp/wtmp file is provided in this release. If you use this, be aware that these files are only updated when an xrdp session is created or destroyed. Disconnections and reconnections to the same session are not tracked. In particular:-
  * the FROM address for a client (as shown by the `w` command) reflects the IP address of the client at the time of creation, and not the address of the currently connected client.
  * Sessions started by the `xrdp-sesrun` command do not have a FROM address.

## Security fixes
None

## New features
* The number of threads assigned to the x264 encoder can now be configured (#3366 #3367)
* The colon in a share name passed from the client can be replaced with another character (#3389)
* Experimental support for utmp/wtmp is backported from devel. Thanks to @mlewissmith for this contribution.
* Add Hungarian keyboard (#3424 #3430)
* Improved keyboard fallback logic for xorgxrdp results in better support for some keyboard variants (e.g. Brazil ABNT2) #3478
* A new session type (Xvnc over Unix Domain Socket) has been added. Although intended primarily for Enterprise FIPS installations which use the Xvnc backend, this can be used with TigerVNC on any platform to improve security (#3453)

## Bug fixes
* Fix potential memory leaks (#3380 #3388)
* Documentation fixes (#3403)
* Various Coverity warnings have been addressed (#3411 #3423)
* xrdp now copes with a mis-installed openh264 encoder (#3405 #3432)
* Bug #2518 which affects FIPS-compliant Enterprise installations can be addressed by using the new 'Xvnc over UDS' session type (#3453)
* FreeBSD: xrdp now avoids creating sessions with the same display number as forwarded X session over ssh (#3381 #3456)


## Internal changes
* FreeBSD CI bumped to 14.3 (#3427 #3685)

## Changes for users
None

## Changes for packagers or developers
* The `--enable-utmp` needs to be added to enable UTMP support.
* The config file subdirectory (`xrdp` part of `/etc/xrdp`) can now be configured (#3369)
* Packagers using TigerVNC to provide the Xvnc backend may wish to configure the 'Xvnc over UDS' session type as a default by using a `code=1` line in xrdp.ini. Instructions are provided in the released xrdp.ini file.

-----------------------

# Release notes for xrdp v0.10.2 (2024/12/24)

## General announcements

[Power Up Privacy](https://powerupprivacy.com/) and @CyberTrust sponsored H.264 encoding (mentioned later).  We greatly appreciate the sponsorship. 

If you like xrdp, please consider sponsoring or donating to the project. We accept financial contributions through [Open Collective](https://opencollective.com/xrdp-project), and direct donations to individual developers via GitHub Sponsors are also welcome.

## Highlights

### H.264 encoding

We’re very excited to announce that xrdp has supported H.264 encoding in graphics remoting since v0.10.2. xrdp with H.264 encoding reduces the amount of data transmitted over the network and provides a much smoother and more responsive experience compared to previous versions when using graphics-intensive applications.

For details, see the [[H.264 encoding]] page on the wiki and also check the [[Known Issues|H.264 encoding#Known-Issues]] section.

### Unprivileged xrdp daemon

Since v0.10.2, xrdp officially supports running `xrdp` daemon as an unprivileged user. `xrdp-sesman` daemon still needs to be run as a privileged user because it handles user authentication and session management.

Running `xrdp` daemon as an unprivileged user requires some adjustments, such as user/group and files/directory permissions. We have bundled a script named `xrdp-chkpriv` with xrdp to check if it is ready to run `xrdp` as an unprivileged user. The script is typically installed into `/usr/share/xrdp/xrdp-chkpriv`. See also the man page of `xrdp.ini` for more configuration information.

## Security fixes
None

## New features
* FUSE operations can now use direct I/O to bypass the block cache (#3260)
* Supported clients can now skip channel join messages (#3282)
* Frame capture interval (frame rate) can now be configured separately via xrdp for H.264 and RFX (neutrinolabs/xorgxrdp#347 #3317)
* The statvfs system call is now supported on the FUSE filesystem (#3304)
* A path can now be specified for the chansrv log file (#3344)
* Add Czech keyboard (#3348 #3358)

## Bug fixes
* Redirector improvements. Removed some unnecessary limitations on filename lengths, and improved compatibility with FreeRDP (#3165 #3194). Special thanks to @tsz8899 for raising this and working with the team.
* Fix misreported cache size (#3212)
* Clarified Policy setting in sesman.ini (#3235)
* Fixed a regression in support for non-resizeable VNC sessions (#3242)
* A regression in chansrv functionality when used in standalone mode for VNC sessions has been fixed (#3283). This was introduced by the move to the v0.10.x sockets dir layout
* Fix AltGr on Spanish keyboard (#3313)
* The KDE Dolphin file manager can now save files to a mapped drive (#3300)
* pam_limits.so is now included explicitly for Debian and derivatives (#3347)

## Internal changes
* CI version updates : cppcheck to v2.15.0 and astyle to 3.4.14 (#3232 #3309 #3314)
* Remove xrdp_sec_in_mcs_data() function (#3273)

## Changes for users

* `xrdp.ini` has some new configuration parameters for H. 264 (#3317). When updating from v0.10.1 to v0.10.2, make sure to merge the new `xrdp.ini` with the old one.
* If moving from v0.9.x, read the v0.10.0 release note.

## Changes for packagers or developers

* `xrdp.ini` and `sesman.ini` are now dynamically substituted during the build process (it was not working as intended before) (#3187 #3188)
* Running xrdp daemon as an unprivileged user is now officially supported. It is optional but consider creating a user/group for `xrdp` daemon in the post-install script or an appropriate location.
* The libfuse version required is now > 3.1.0 (#3284)
* If moving from v0.9.x, read the v0.10.0 release note.

-----------------------

# Release notes for xrdp v0.10.1 (2024/07/31)

## General announcements

A clipboard bugfix included in this release is sponsored by Krämer Pferdesport GmbH & Co KG. We very much appreciate the sponsorship.

Please consider sponsoring or making a donation to the project if you like xrdp. We accept financial contributions via [Open Collective](https://opencollective.com/xrdp-project). Direct donations to each developer via GitHub Sponsors are also welcomed.

## Security fixes
* Unauthenticated RDP security scan finding / partial auth bypass (no CVE). Thanks to @txtdawg for reporting this.

## New features
* GFX-RFX lossy compression levels are now selectable depending on connection type on the client (#3183, backport of #2973)

## Bug fixes
* A regression in the code for creating the chansrv FUSE directory has been fixed (#3088, backport of #3082)
* Fix a systemd dependency ("network-online.target") (#3088, backport of #3086)
* A problem in session list processing which could result in incorrect display assignments has been fixed (#3088, backport of #3103)
* A problem in GFX resizing which could lead to a SEGV in xrdp has been fixed (#3088, backport of #3107)
* A problem with the US Dvorak keyboard layout has been resolved (#3088, backport of #3112)
* A regression bug when pasting image to LibreOffice has been fixed [Sponsored by Krämer Pferdesport GmbH & Co KG] (#3102 #3120)
* Fix a regression when the server tries to negotiate GFX when max_bpp is not high enough (#3118 #3122)
* Fix a GFX multi-monitor screen placing issue on minimise/maximize (#3075 #3127)
* Fix an issue some files are not included properly in release tarball (#3149 #3150)
* Using 'I' in the session selection policy now works correctly (#3167 #3171)
* A potential name buffer overflow in the redirector has been fixed [no security implications] (#3175)
* Screens wider than 4096 pixels should now be supported (#3083)
* An unnecessary licensing exchange during connection setup has been removed. This was causing problems for FIPS-compliant clients (#3132  backport of #3143)

## Internal changes
* FreeBSD CI bumped to 14.2 (#3088 #3427)

## Changes for users
* None since v0.10.0.
* If moving from v0.9.x, read the v0.10.0 release note.

## Changes for packagers or developers
* None since v0.10.0.
* If moving from v0.9.x, read the v0.10.0 release note.

-----------------------

# Release notes for xrdp v0.10.0 (2024/05/10)

This section notes changes since the [v0.10 branch](#branch-v010) was created. 

## General announcements
The biggest news of this release is that [Graphic Pipeline Extension](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-rdpegfx/da5c75f9-cd99-450c-98c4-014a496942b0) also called GFX in short has been supported. xrdp v0.10 with GFX achieves more frame rates and less bandwidth compared to v0.9. There is a significant performance improvement especially if the client is Windows 11's mstsc.exe or Microsoft Remote Desktop for Mac. GFX H.264/AVC 444 mode and hardware-accelerated encoding are not supported in this version yet.

GFX implementation in xrdp is sponsored by an enterprise sponsor. @CyberTrust is also one of the sponsors. We very much appreciate the sponsorship. It helped us to accelerate xrdp development and land GFX earlier! 

Please consider sponsoring or making a donation to the project if you like xrdp. We accept financial contributions via [Open Collective](https://opencollective.com/xrdp-project). Direct donations to each developer via GitHub Sponsors are also welcomed.

## Highlights
This section describes the most user-visible new or changed features in xrdp since v0.9.19. See [Branch v0.10](#branch-v010) for all changes relative to v0.9.19.

* Added GFX support with multi-monitor support (including monitor hot plug/unplug) (#2256 #2338 #2595 #2879 #2891 #2911 #2929 #2933)
* Touchpad inertial scrolling (#2364, #2424). Thanks to new contributor @seflerZ
* New look of login screen (#2366)
* Scaled login screen on higher DPI monitors (#2341, #2427, #2435)
   * This feature works automatically when monitor DPI information is sent by the client (i.e. a full-screen session)
   * Native platform tools are now provided to manipulate .fv1 format font files.
* The format of the date and time in the log file has been changed to ISO 8601 with milliseconds (#2386 #2541)
* xrdp-sesman now supports a `--reload` switch to allow for the configuration to be changed when sessions are active (#2416)

## Security fixes
None

## New features
* If the client announces support for the Image RemoteFX codec it is logged (back-port of #2946)

## Bug fixes
* Fix some monitor hotplug issues (#2951)
* GFX: Fix disconnect on resize of busy windows (#2962 #2957)
* Fall back to IPv4 if IPv6 capable but don't have an IPv6 address set (#2967 #2957)
* Remove tcutils channel from xrdp.ini (#2970 #2957)
* Don't generate a corefile when generating SIGSEGV during unit testing (#2987)
* If the drdynvc static channel isn't available, disable GFX gracefully (#3003)
* A buffer misconfiguration which affects performance on high bandwidth, high latency links has been addressed (cherry-pick of #2910)
* A permissions fix for the socketdir update in #2731 has been issued (cherry-pick of #3011)

## Internal changes
* Adjust log level not too verbose (#2954 #2972 #2957)
* Migrate GitHub actions to Node 20 (#2955 #2957)
* Bump copyright year and make easier to bump (#2956 #2957)
* Remove duplicate DEBUG output (#2976 #2977)
* Add script to make release tarball (#2983)
* Syscall filter for xrdp updated (cherry-pick of #3017)
* GFX memory usage for large screens is greatly improved (cherry-pick of #3013)
* librfxcodec SSE2 performance improvements (#3032)

## Known issues
* On-the-fly resolution change with the Microsoft Store version of Remote Desktop client sometimes crashes on connect (#1869)
* xrdp's login dialog is not relocated at the center of the new resolution after on-the-fly resolution change happens (#1867)

## Changes for users
* If moving from v0.9.x, read the '[User changes](#user-changes)' for the v0.10 branch below.

## Changes for packagers or developers
* If moving from v0.9.x, read the '[User changes](#user-changes)'  and '[Significant changes for packagers or developers section](#significant-changes-for-packagers-or-developers)' sections for the v0.10 branch below.

-----------------------

# Branch v0.10

This branch was forked from development on 2024-02-08 in preparation for testing and release of v0.10.1.

The changes in this section are relative to version v0.9.19 of xrdp.

## User changes
* The [x11rdp](/neutrinolabs/x11rdp) X server is no longer supported. Users will need to move to xorgxrdp (#2489)
* Running xrdp and xrdp-sesman on separate hosts is no longer supported.
* There are some changes to `xrdp.ini` and `sesman.ini` which break backwards compatibility. In particular:-
   * `sesman.ini/Globals/ListenAddress` is not longer used. A warning message is generated if this is found in the configuration, but the configuration will continue to work.
   * `sesman.ini/Globals/ListenPort` is now a path to a socket, or an unqualified socket in a default directory. If the old default value `3350` is found, a warning is generated and a default value is used instead. The configuration will continue to work.
   * The `ip` and `pamsessionmng` parameters are no longer required in sections in `xrdp.ini` to locate the sesman port. Unnecessary usages of this parameter now generate warnings. The configuration will continue to work.
   * The 'C' field for the session allocation policy has been replaced with `Policy=Separate`. This field is has a very specific specialist purpose, and will not be used by the vast majority of users. The renaming makes it much clearer what is happening (#2251 #2239). Any uses of the 'C' field will generate warnings, **and the configuration will require updating**
* The format of the date and time in the log file has been changed to ISO 8601 with milliseconds (#2386 #2541)

   Users are urged to heed any generated configuration warnings and update their configurations. Later major versions of xrdp may remove these warnings, or introduce other behaviours for the affected parameters.

## Security fixes 

This branch provides following important security fixes reported by [Team BT5 (BoB 11th)](https://github.com/Team-BT5). We appreciate their great help with making and reviewing patches for them.

* [CVE-2022-23468](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-23468)
* [CVE-2022-23477](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-23477)
* [CVE-2022-23478](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-23478)
* [CVE-2022-23479](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-23479)
* [CVE-2022-23480](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-23480)
* [CVE-2022-23481](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-23481)
* [CVE-2022-23483](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-23483)
* [CVE-2022-23482](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-23482)
* [CVE-2022-23484](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-23484)
* [CVE-2022-23493](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-23493)

The following issue was reported by [@gafusss](https://github.com/gafusss)
* [CVE-2023-40184](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2023-40184)

Other security fixes:-
* [CVE-2023-42822: Unchecked access to font glyph info](https://www.cve.org/CVERecord?id=CVE-2023-42822)

## New features
* Added GFX support with multi-monitor support (including monitor hot plug/unplug) (#2256 #2338 #2595 #2879 #2891 #2911 #2929 #2933)
* Add Ulalaca that enables remote access to macOS's native screen (developed by [team unstablers](https://unstabler.pl/))
   * Ulalaca is still heavy in development, not suitable for production use yet
   * `sessionbroker` and `sessionprojector` are also required, see also [README](https://github.com/unstabler/ulalaca)
* Scaled login screen on higher DPI monitors (#2341, #2427, #2435)
   * This feature works automatically when monitor DPI information is sent by the client (i.e. a full-screen session)
   * Native platform tools are now provided to manipulate .fv1 format font files.
* Touchpad inertial scrolling (#2364, #2424). Thanks to new contributor @seflerZ
* New look of login screen (#2366)
* Record codec GUID to identify unknown codc (#2401)
* OpenSuSE Tumbleweed move to /usr/lib/pam.d is now supported in the installation scripts (#2413)
* xrdp-sesman now supports a `--reload` switch to allow for the configuration to be changed when sessions are active (#2416)
* VNC backend session now supports extra mouse buttons 6, 7 and 8 (#2426)
* `LogFile=<stdout>` redirects log to stdout, which is useful for debugging (#2407)
* xrdp-sesrun and xrdp-sesadmin can now authenticate automatically as the logged-in user without a password (#2472)
* Empty passwords are no longer automatically passed though to sesman for authentication (#2487)
* BSD setusercontext() is now supported (#2225, #2473)
* The FUSE mount path can now be qualified with the display name or display string (#2528)
* Debian: use startup command from /usr/share/xsession if DISPLAY_SESSION is set (#2522)
* The directory where PAM configuration files is installed can now be set with `--with-pamconfdir` (#2552 #2557)
* Some classes of 'blue screen' failures have been addressed:-
   * X server failures are now reported as a separate error from window manager (#2592)
   * sesman failures are reported immediately (#2640)
* Allow longer UserWindowManager strings (#2651)
* Some changes have been made to made it easier to implement AppArmor support in the future (#2265):-
   * `g_file_open()` has been replaced with `g_file_open_ro()` and `g_file_open_rw()` calls
   * the starting of the X server with no-new-privileges can now be disabled by the administrator
* On systemd-based systems, system call filtering is used to restrict the system calls that the xrdp process can make (#2697 #2719)
* GNOME and KDE keyrings should now be supported out-of-the-box on Debian and Arch (#2776)
* Implement vsock support for FreeBSD #2798 
* Side buttons on some mice are now supported by NeutrinoRDP (#2864). Thanks to new contributor @naruhito for this patch.
* Support for the Elbrus E2K architecture (#2872). Thanks to new contributor @r-a-sattarov for this patch.
* Just log Image RemoteFX codec (#2946)

## Bug fixes
* A missing directive to link libxrdpapi with libcommon has been added (#2185)
* Some sesman config warning messages could be lost. This has now been fixed (#2198)
* Moving sesman to a Unix domain socket fixes a number of issues related to firewalls and Ipv4/v6 connectivity issues (#1596 #1805 #1855)
* Secondary groups are now added correctly on Linux from /etc/security/group.conf (#1978).
* The `--disable-static` switch for `configure` now works (#1467 #2257)
* Windows RDS compatibility has been improved, so some old clients (e.g. Wyse Sx0) can now be used again with xrdp in non-TLS mode (#2166)
* PAM_RHOST is now set for the PAM stack (#2251, #392)
* A minor spacing issue in a sesman log message has been fixed (#2282)
* MSTSC crashes when resolution is changed by maximizing on a different monitor (#2291 #2300)
* Fix swapped `require_credentials`/`enable_token_login` config options in xrdp.ini manpage (#2391)
* Passwords are no longer left on the heap in sesman (#1599 #2438)
* Set permissions on pcsc socket dir to owner only (#2454 #2459)
* The Kerberos authentication module has been reworked and tested (#2453)
* Minor documentation fixes (#2481 #2581)
* The correct message is now generated when the session limit is reached (#642)
* sesman now returns better information to xrdp when session creation fails (#909, #1921)
* MaxLoginRetry limit for sesman now works (#1739)
* On systems where the same user can have multiple names, the correct session is now reconnected (#1823)
* On FreeBSD, the correct peer is now logged for xorgxrdp connections (#146)
* In some situations, xrdp sessions can be bootstrapped on system startup (#1303)
* Don't try to listen on the scard socket if it isn't there (#2504)
* Fix some noise of MP3/AAC audio redirection and add some parameters to tweak sounds (#2519 #2608)
* Memory management fixes to list module (#2536 #2575)
* Session is not now started until X server is fully active (#2492)
* Fix potential NULL dereferences in chansrv (#2573)
* An erroneous free in the smartcard handling code has been removed (#2607)
* An unnecessary 'check.h' include was removed which prevented compilation on Arch systems (#2649)
* No user session created with xrdp and pam_systemd_home account module (#1684)
* Homedir gets not correctly created at first login (#350)
* pam_setcred never returns xrdp-sesman is hung (#1323)
* chansrv should no longer hang occasionally in developer builds on session exit (#2145)
* Environment variables set by PAM modules are no longer restricted to around 250 characters (#2711)
* Checking group membership should now work better on systems using directory services (#2806 #2815)
* Pasting more than 32K characters of text to the clipboard now succeeds (#1839 #2810)
* An incompatibility with FreeRDP 2.11.2 in the drive redirector has been fixed (#2834 #2838)
* Unicode bugs have been fixed (#942 #2603)

## Internal changes
* SCP (Sesman Control Protocol) has been refactored from separate V0 and V1 protocols to a simplified V2 protocol running on top of a new library 'libipm' (#2163).
* libipm provides a way to pass file descriptors between processes (#2494)
* SCP connections are now only supported on top of Unix Domain Sockets (#2207 #2235 #2247)
* Monitor processing logic, which was in two places, has now been unified (#1895 #2301)
* Simplifications to transport connect logic (#2204)
* The fields in `struct trans` and `struct xrdp_client_info` used for storing client addressing information have been simplified (#2251)
* A couple of string utility functions have been added to parse character strings like the one used for the session allocation policy (#2251)
* cppcheck version used for CI bumped to 2.13.0 (#2520 #2737 #2785 #2886). Note that #2785 greatly increases cppcheck scan times.
* cppcheck install script no longer installs z3 for cppcheck >= 2.8 (#2782)
* The physical desktop size information sent from the client is now recorded in more situations (#2310)
* Simple maintenance improvements (#2354)
* An opaque type is now used for the auth_info handle used by the sesman auth module (#2362)
* CI updates to cope with github upgrades (#2394)
* GUIDs created for new sessions are now compliant with RFC4122 random UUIDs (#2420)
* Some 'magic numbers' have been replaced with constants (#2421)
* FreeBSD CI now runs a 'make check' (#2490)
* FreeBSD CI now runs on FreeBSD 13.2 (#2621 #2896)
* Some logging improvements on audio redirection (#2537)
* Extra executables : waitforx (#2492 #2591 #2586) xrdp-sesexec (#2644)
* The poll() system call now replaces select() for monitoring file descriptors (#2497 #2568)
* sigaction() now replaces signal() for increased portability (#2813)
* Other portability changes (#2909)
* Some extra convenience functions were added for handling lists of strings (#2576)
* `g_malloc`, `g_free`, `g_memset`, `g_memcpy`, and `g_memmove` are now macros. These should not be used in new code (#2609)
* config_ac.h is now used consistently (#2667)
* as mentioned above, `g_file_open()` has been replaced with `g_file_open_ro()` and `g_file_open_rw()` calls
* The separate fifo packages in the common directory and chansrv have now been merged (#2686)
* Unicode conversions are now provided by explicit functions rather than relying on C library `mbstowcs()`/`wcstombs()` functions (#2794)
* Some test timeouts have been increased for slow CI machines (#2901)
* `g_obj_wait()` can now take a zero timeout (#2904)
* POSIX shared memory is now used to communicate with `xorgxrdp` rather than System-V shared memory (#2709 #2786 #2889)

## Significant changes for packagers or developers
* The libscp.so shared library is replaced with libipm.so
* A new shared library libsesman.so contains shared code for sesman and related executables (#2601)
* The default setting for `--with-socketdir` is now `/var/run/xrdp` rather than `/tmp/.xrdp`. The new setting works for installations where `/tmp` is polyinstantiated ( see #1482 for more details)
* The permissions of the socketdir have changed from 1777 to 755 (owned by root). Within this directory are the sesman socket and user-specific directories. The user-specific directories store the session sockets used by each user (#2731).  
   It is recommended not to use the same `--with-socketdir` setting for v0.9.x and v0.10.x packages as the differing permissions can cause problems on package downgrades. See #3066 for an example of where this can be a problem.
* Passing `--disable-static` to `configure` prevents unused static libraries being installed by `make install`.
* The `simple.c` example xrdpapi program has been updated to work with logging changes, and is now built as part of the CI (#2276)
* If the `xrdp-mkfv1` utility is to be built, the switch `--with-freetype2` must be passed to `./configure`.
* Minimum supported autoconf version is now 2.69 (#2408)
* Add xrdp-sesman.system to distributed files (#2466 #2467)
* A developer-only utility to exercise the auth module selected at configure time has been provided (#2453)
* Extra executables have been added to this release in pkglibexecdir
* The default systemd unit files have been changed to no longer fork (#2672)