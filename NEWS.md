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