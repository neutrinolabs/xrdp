/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * MS-RDPBCGR : Definitions from [MS-RDPBCGR]
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * References to MS-RDPBCGR are currently correct for v20190923 of that
 * document
 */

#if !defined(MS_RDPBCGR_H)
#define MS_RDPBCGR_H

/* RDP Security Negotiation codes */
#define RDP_NEG_REQ                    0x01  /* MS-RDPBCGR 2.2.1.1.1 */
#define RDP_NEG_RSP                    0x02  /* MS-RDPBCGR 2.2.1.2.1 */
#define RDP_NEG_FAILURE                0x03  /* MS-RDPBCGR 2.2.1.2.2 */
#define RDP_CORRELATION_INFO           0x06  /* MS-RDPBCGR 2.2.1.1.2 */

/* Protocol types codes (2.2.1.1.1, 2.2.1.2.1) */
#define PROTOCOL_RDP                   0x00000000
#define PROTOCOL_SSL                   0x00000001
#define PROTOCOL_HYBRID                0x00000002
#define PROTOCOL_RDSTLS                0x00000004
#define PROTOCOL_HYBRID_EX             0x00000008

/* Negotiation packet flags (2.2.1.2.1) */
#define EXTENDED_CLIENT_DATA_SUPPORTED            0x01
#define DYNVC_GFX_PROTOCOL_SUPPORTED              0x02
#define NEGRSP_RESERVED                           0x04
#define RESTRICTED_ADMIN_MODE_SUPPORTED           0x08
#define REDIRECTED_AUTHENTICATION_MODE_SUPPORTED  0x10

/* RDP Negotiation Failure Codes (2.2.1.2.2) */
#define SSL_REQUIRED_BY_SERVER                 0x00000001
#define SSL_NOT_ALLOWED_BY_SERVER              0x00000002
#define SSL_CERT_NOT_ON_SERVER                 0x00000003
#define INCONSISTENT_FLAGS                     0x00000004
#define HYBRID_REQUIRED_BY_SERVER              0x00000005
#define SSL_WITH_USER_AUTH_REQUIRED_BY_SERVER  0x00000006

/* TS_UD_HEADER: type ((2.2.1.3.1) */
/* TODO: to be renamed */
#define SEC_TAG_CLI_INFO       0xc001 /* CS_CORE? */
#define SEC_TAG_CLI_CRYPT      0xc002 /* CS_SECURITY? */
#define SEC_TAG_CLI_CHANNELS   0xc003 /* CS_CHANNELS? */
#define SEC_TAG_CLI_4          0xc004 /* CS_CLUSTER? */
#define SEC_TAG_CLI_MONITOR    0xc005 /* CS_MONITOR */
#define SEC_TAG_CLI_MONITOR_EX 0xc008 /* CS_MONITOR_EX */

/* Client Core Data: colorDepth, postBeta2ColorDepth (2.2.1.3.2) */
#define RNS_UD_COLOR_4BPP      0xCA00
#define RNS_UD_COLOR_8BPP      0xCA01
#define RNS_UD_COLOR_16BPP_555 0xCA02
#define RNS_UD_COLOR_16BPP_565 0xCA03
#define RNS_UD_COLOR_24BPP     0xCA04

/* Client Core Data: supportedColorDepths (2.2.1.3.2) */
#define RNS_UD_24BPP_SUPPORT 0x0001
#define RNS_UD_16BPP_SUPPORT 0x0002
#define RNS_UD_15BPP_SUPPORT 0x0004
#define RNS_UD_32BPP_SUPPORT 0x0008

/* Client Core Data: earlyCapabilityFlags (2.2.1.3.2) */
#define RNS_UD_CS_WANT_32BPP_SESSION         0x0002
#define RNS_UD_CS_SUPPORT_MONITOR_LAYOUT_PDU 0x0040
#define RNS_UD_CS_SUPPORT_DYNVC_GFX_PROTOCOL 0x0100

/* Client Core Data: connectionType  (2.2.1.3.2) */
#define CONNECTION_TYPE_MODEM          0x01
#define CONNECTION_TYPE_BROADBAND_LOW  0x02
#define CONNECTION_TYPE_SATELLITE      0x03
#define CONNECTION_TYPE_BROADBAND_HIGH 0x04
#define CONNECTION_TYPE_WAN            0x05
#define CONNECTION_TYPE_LAN            0x06
#define CONNECTION_TYPE_AUTODETECT     0x07

/* TS_UD_CS_NET (2.2.1.3.4) */
/* This isn't explicitly named in MS-RDPBCGR */
#define MAX_STATIC_CHANNELS            31

/* Channel definition structure CHANNEL_DEF (2.2.1.3.4.1) */
#define CHANNEL_NAME_LEN                7
/* These names are also not explicitly defined in MS-RDPBCGR */
#define CLIPRDR_SVC_CHANNEL_NAME        "cliprdr"
#define DRDYNVC_SVC_CHANNEL_NAME        "drdynvc"
#define RAIL_SVC_CHANNEL_NAME           "rail"
#define RDPSND_SVC_CHANNEL_NAME         "rdpsnd"
#define RDPDR_SVC_CHANNEL_NAME          "rdpdr"

/* 2.2.1.3.6 Client Monitor Data */
/* monitorCount (4 bytes): A 32-bit, unsigned integer. The number of display */
/* monitor definitions in the monitorDefArray field (the maximum allowed is 16). */
#define CLIENT_MONITOR_DATA_MAXIMUM_MONITORS               16

/* 2.2.1.3.6 Client Monitor Data */
/* The maximum width of the virtual desktop resulting from the union of the monitors */
/* contained in the monitorDefArray field MUST NOT exceed 32,766 pixels. Similarly, */
/* the maximum height of the virtual desktop resulting from the union of the monitors  */
/* contained in the monitorDefArray field MUST NOT exceed 32,766 pixels. */
/* The minimum permitted size of the virtual desktop is 200 x 200 pixels. */
#define CLIENT_MONITOR_DATA_MINIMUM_VIRTUAL_DESKTOP_WIDTH  0xC8   // 200
#define CLIENT_MONITOR_DATA_MINIMUM_VIRTUAL_DESKTOP_HEIGHT 0xC8   // 200
#define CLIENT_MONITOR_DATA_MAXIMUM_VIRTUAL_DESKTOP_WIDTH  0x7FFE // 32766
#define CLIENT_MONITOR_DATA_MAXIMUM_VIRTUAL_DESKTOP_HEIGHT 0x7FFE // 32766

/* 2.2.1.3.6.1 Monitor Definition (TS_MONITOR_DEF) */
#define TS_MONITOR_PRIMARY 0x00000001

/* Options field */
/* NOTE: XR_ prefixed to avoid conflict with FreeRDP */
#define XR_CHANNEL_OPTION_INITIALIZED   0x80000000
#define XR_CHANNEL_OPTION_ENCRYPT_RDP   0x40000000
#define XR_CHANNEL_OPTION_ENCRYPT_SC    0x20000000
#define XR_CHANNEL_OPTION_ENCRYPT_CS    0x10000000
#define XR_CHANNEL_OPTION_PRI_HIGH      0x08000000
#define XR_CHANNEL_OPTION_PRI_MED       0x04000000
#define XR_CHANNEL_OPTION_PRI_LOW       0x02000000
#define XR_CHANNEL_OPTION_COMPRESS_RDP  0x00800000
#define XR_CHANNEL_OPTION_COMPRESS      0x00400000
#define XR_CHANNEL_OPTION_SHOW_PROTOCOL 0x00200000
#define REMOTE_CONTROL_PERSISTENT       0x00100000

/* Server Proprietary Certificate (2.2.1.4.3.1.1) */
/* TODO: to be renamed */
#define SEC_TAG_PUBKEY                 0x0006 /* BB_RSA_KEY_BLOB */
#define SEC_TAG_KEYSIG                 0x0008 /* BB_SIGNATURE_KEY_BLOB */

/* Info Packet (TS_INFO_PACKET): flags (2.2.1.11.1.1) */
/* TODO: to be renamed */
#define RDP_LOGON_AUTO                 0x0008
#define RDP_LOGON_NORMAL               0x0033
#define RDP_COMPRESSION                0x0080
#define RDP_LOGON_BLOB                 0x0100
#define RDP_LOGON_LEAVE_AUDIO          0x2000
#define RDP_LOGON_RAIL                 0x8000

/* Extended Info Packet: clientAddress (2.2.1.11.1.1.1) */
#define EXTENDED_INFO_MAX_CLIENT_ADDR_LENGTH 80

/* Extended Info Packet: performanceFlags (2.2.1.11.1.1.1) */
/* TODO: to be renamed */
#define RDP5_DISABLE_NOTHING           0x00
#define RDP5_NO_WALLPAPER              0x01
#define RDP5_NO_FULLWINDOWDRAG         0x02
#define RDP5_NO_MENUANIMATIONS         0x04
#define RDP5_NO_THEMING                0x08
#define RDP5_NO_CURSOR_SHADOW          0x20
#define RDP5_NO_CURSORSETTINGS         0x40 /* disables cursor blinking */

/* LICENSE_BINARY_BLOB (2.2.1.12.1.2) */
#define LICENCE_TAG_USER               0x000f /* BB_CLIENT_USER_NAME_BLOB */
#define LICENCE_TAG_HOST               0x0010 /* BB_CLIENT_MACHINE_NAME_BLOB */

/* Maps to generalCapabilitySet in T.128 page 138 */

/* Capability Set: capabilitySetType (2.2.1.13.1.1.1) */
#define CAPSTYPE_GENERAL                        0x0001
#define CAPSTYPE_GENERAL_LEN                    0x18

#define CAPSTYPE_BITMAP                         0x0002
#define CAPSTYPE_BITMAP_LEN                     0x1C

#define CAPSTYPE_ORDER                          0x0003
#define CAPSTYPE_ORDER_LEN                      0x58
#define ORDER_CAP_NEGOTIATE                     2 /* NEGOTIATEORDERSUPPORT? not used */
#define ORDER_CAP_NOSUPPORT                     4 /* not used */

#define CAPSTYPE_BITMAPCACHE                    0x0004
#define CAPSTYPE_BITMAPCACHE_LEN                0x28

#define CAPSTYPE_CONTROL                        0x0005
#define CAPSTYPE_CONTROL_LEN                    0x0C

#define CAPSTYPE_ACTIVATION                     0x0007
#define CAPSTYPE_ACTIVATION_LEN                 0x0C

#define CAPSTYPE_POINTER                        0x0008
#define CAPSTYPE_POINTER_LEN                    0x0a
#define CAPSTYPE_POINTER_MONO_LEN               0x08

#define CAPSTYPE_SHARE                          0x0009
#define CAPSTYPE_SHARE_LEN                      0x08

#define CAPSTYPE_COLORCACHE                     0x000A
#define CAPSTYPE_COLORCACHE_LEN                 0x08

#define CAPSTYPE_SOUND                          0x000C

#define CAPSTYPE_INPUT                          0x000D
#define CAPSTYPE_INPUT_LEN                      0x58

#define CAPSTYPE_FONT                           0x000E
#define CAPSTYPE_FONT_LEN                       0x04

#define CAPSTYPE_BRUSH                          0x000F
#define CAPSTYPE_BRUSH_LEN                      0x08

#define CAPSTYPE_GLYPHCACHE                     0x0010
#define CAPSTYPE_OFFSCREENCACHE                 0x0011

#define CAPSTYPE_BITMAPCACHE_HOSTSUPPORT        0x0012
#define CAPSTYPE_BITMAPCACHE_HOSTSUPPORT_LEN    0x08

#define CAPSTYPE_BITMAPCACHE_REV2               0x0013
#define CAPSTYPE_BITMAPCACHE_REV2_LEN           0x28
#define BMPCACHE2_FLAG_PERSIST                  ((long)1<<31)

#define CAPSTYPE_VIRTUALCHANNEL                 0x0014
#define CAPSTYPE_VIRTUALCHANNEL_LEN             0x08

#define CAPSTYPE_DRAWNINGRIDCACHE               0x0015
#define CAPSTYPE_DRAWGDIPLUS                    0x0016
#define CAPSTYPE_RAIL                           0x0017
#define CAPSTYPE_WINDOW                         0x0018

#define CAPSSETTYPE_COMPDESK                    0x0019
#define CAPSSETTYPE_COMPDESK_LEN                0x06

#define CAPSSETTYPE_MULTIFRAGMENTUPDATE         0x001A
#define CAPSSETTYPE_MULTIFRAGMENTUPDATE_LEN     0x08

#define CAPSETTYPE_LARGE_POINTER                0x001B
#define CAPSETTYPE_LARGE_POINTER_LEN            0x06

#define CAPSETTYPE_SURFACE_COMMANDS             0x001C
#define CAPSETTYPE_SURFACE_COMMANDS_LEN         0x0C

#define CAPSSETTYPE_BITMAP_CODECS               0x001D
#define CAPSSETTYPE_BITMAP_CODECS_LEN           0x1C

#define CAPSTYPE_FRAME_ACKNOWLEDGE              0x001E
#define CAPSTYPE_FRAME_ACKNOWLEDGE_LEN          0x08

/* Control PDU Data: action (2.2.1.15.1) */
/* TODO: to be renamed */
#define RDP_CTL_REQUEST_CONTROL        1 /* CTRLACTION_REQUEST_CONTROL */
#define RDP_CTL_GRANT_CONTROL          2
#define RDP_CTL_DETACH                 3
#define RDP_CTL_COOPERATE              4

/* RDP5 disconnect PDU */
/* Set Error Info PDU Data: errorInfo (2.2.5.1.1) */
/* TODO: to be renamed */
#define exDiscReasonNoInfo                            0x0000
#define exDiscReasonAPIInitiatedDisconnect            0x0001
#define exDiscReasonAPIInitiatedLogoff                0x0002
#define exDiscReasonServerIdleTimeout                 0x0003
#define exDiscReasonServerLogonTimeout                0x0004
#define exDiscReasonReplacedByOtherConnection         0x0005
#define exDiscReasonOutOfMemory                       0x0006
#define exDiscReasonServerDeniedConnection            0x0007
#define exDiscReasonServerDeniedConnectionFips        0x0008
#define exDiscReasonLicenseInternal                   0x0100
#define exDiscReasonLicenseNoLicenseServer            0x0101
#define exDiscReasonLicenseNoLicense                  0x0102
#define exDiscReasonLicenseErrClientMsg               0x0103
#define exDiscReasonLicenseHwidDoesntMatchLicense     0x0104
#define exDiscReasonLicenseErrClientLicense           0x0105
#define exDiscReasonLicenseCantFinishProtocol         0x0106
#define exDiscReasonLicenseClientEndedProtocol        0x0107
#define exDiscReasonLicenseErrClientEncryption        0x0108
#define exDiscReasonLicenseCantUpgradeLicense         0x0109
#define exDiscReasonLicenseNoRemoteConnections        0x010a

/* Virtual channel PDU (2.2.6.1) */
#define CHANNEL_CHUNK_LENGTH                          1600

/* Channel PDU Header flags (2.2.6.1.1) */
/* NOTE: XR_ prefixed to avoid conflict with FreeRDP */
#define XR_CHANNEL_FLAG_FIRST          0x00000001
#define XR_CHANNEL_FLAG_LAST           0x00000002
#define XR_CHANNEL_FLAG_SHOW_PROTOCOL  0x00000010

/* General Capability Set: osMajorType (2.2.7.1.1) */
#define OSMAJORTYPE_UNSPECIFIED        0x0000
#define OSMAJORTYPE_WINDOWS            0x0001
#define OSMAJORTYPE_OS2                0x0002
#define OSMAJORTYPE_MACINTOSH          0x0003
#define OSMAJORTYPE_UNIX               0x0004
#define OSMAJORTYPE_IOS                0x0005
#define OSMAJORTYPE_OSX                0x0006
#define OSMAJORTYPE_ANDROID            0x0007
#define OSMAJORTYPE_CHROME_OS          0x0008

/* General Capability Set: osMinorType (2.2.7.1.1) */
#define OSMINORTYPE_UNSPECIFIED        0x0000
#define OSMINORTYPE_WINDOWS_31X        0x0001
#define OSMINORTYPE_WINDOWS_95         0x0002
#define OSMINORTYPE_WINDOWS_NT         0x0003
#define OSMINORTYPE_OS2_V21            0x0004
#define OSMINORTYPE_POWER_PC           0x0005
#define OSMINORTYPE_MACINTOSH          0x0006
#define OSMINORTYPE_NATIVE_XSERVER     0x0007
#define OSMINORTYPE_PSEUDO_XSERVER     0x0008
#define OSMINORTYPE_WINDOWS_RT         0x0009

/*  General Capability Set: protocolVersion (2.2.7.1.1) */
#define  TS_CAPS_PROTOCOLVERSION       0x0200

/* General Capability Set: extraFlags (2.2.7.1.1) */
#define  FASTPATH_OUTPUT_SUPPORTED     0x0001
#define  NO_BITMAP_COMPRESSION_HDR     0x0400
#define  LONG_CREDENTIALS_SUPPORTED    0x0004
#define  AUTORECONNECT_SUPPORTED       0x0008
#define  ENC_SALTED_CHECKSUM           0x0010

/* Order Capability Set: orderSupportExFlags (2.2.7.1.3)  */
/* NOTE: XR_ prefixed to avoid conflict with FreeRDP */
#define XR_ORDERFLAGS_EX_CACHE_BITMAP_REV3_SUPPORT   0x0002
#define XR_ORDERFLAGS_EX_ALTSEC_FRAME_MARKER_SUPPORT 0x0004

/* Order Capability Set: orderFlags (2.2.7.1.3) */
#define  NEGOTIATEORDERSUPPORT         0x0002
#define  ZEROBOUNDSDELTASUPPORT        0x0008
#define  COLORINDEXSUPPORT             0x0020
#define  SOLIDPATTERNBRUSHONLY         0x0040
#define  ORDERFLAGS_EXTRA_FLAGS        0x0080

/* Order Capability Set: orderSupport (2.2.7.1.3) */
#define TS_NEG_DSTBLT_INDEX             0x00
#define TS_NEG_PATBLT_INDEX             0x01
#define TS_NEG_SCRBLT_INDEX             0x02
#define TS_NEG_MEMBLT_INDEX             0x03
#define TS_NEG_MEM3BLT_INDEX            0x04
/*                                      0x05 */
/*                                      0x06 */
#define TS_NEG_DRAWNINEGRID_INDEX       0x07
#define TS_NEG_LINETO_INDEX             0x08
#define TS_NEG_MULTI_DRAWNINEGRID_INDEX 0x09
/*                                      0x0A */
#define TS_NEG_SAVEBITMAP_INDEX         0x0B
/*                                      0x0C */
/*                                      0x0D */
/*                                      0x0E */
#define TS_NEG_MULTIDSTBLT_INDEX        0x0F
#define TS_NEG_MULTIPATBLT_INDEX        0x10
#define TS_NEG_MULTISCRBLT_INDEX        0x11
#define TS_NEG_MULTIOPAQUERECT_INDEX    0x12
#define TS_NEG_FAST_INDEX_INDEX         0x13
#define TS_NEG_POLYGON_SC_INDEX         0x14
#define TS_NEG_POLYGON_CB_INDEX         0x15
#define TS_NEG_POLYLINE_INDEX           0x16
/*                                      0x17 */
#define TS_NEG_FAST_GLYPH_INDEX         0x18
#define TS_NEG_ELLIPSE_SC_INDEX         0x19
#define TS_NEG_ELLIPSE_CB_INDEX         0x1A
#define TS_NEG_INDEX_INDEX              0x1B
/*                                      0x1C */
/*                                      0x1D */
/*                                      0x1E */
/*                                      0x1F */

/* Input Capability Set: inputFlags (2.2.7.1.6) */
#define  INPUT_FLAG_SCANCODES          0x0001
#define  INPUT_FLAG_MOUSEX             0x0004
#define  INPUT_FLAG_FASTPATH_INPUT     0x0008
#define  INPUT_FLAG_UNICODE            0x0010
#define  INPUT_FLAG_FASTPATH_INPUT2    0x0020
#define  INPUT_FLAG_UNUSED1            0x0040
#define  INPUT_FLAG_UNUSED2            0x0080
#define  TS_INPUT_FLAG_MOUSE_HWHEEL    0x0100
#define  TS_INPUT_FLAG_QOE_TIMESTAMPS  0x0200

/* Glyph Cache Capability Set: GlyphSupportLevel (2.2.7.1.8) */
#define GLYPH_SUPPORT_NONE             0x0000
#define GLYPH_SUPPORT_PARTIAL          0x0001
#define GLYPH_SUPPORT_FULL             0x0002
#define GLYPH_SUPPORT_ENCODE           0x0003

/* Desktop Composition Capability Set: CompDeskSupportLevel (2.2.7.2.8) */
#define  COMPDESK_NOT_SUPPORTED      0x0000
#define  COMPDESK_SUPPORTED          0x0001

/* Surface Commands Capability Set: cmdFlags (2.2.7.2.9) */
#define SURFCMDS_SETSURFACEBITS      0x00000002
#define SURFCMDS_FRAMEMARKER         0x00000010
#define SURFCMDS_STREAMSUFRACEBITS   0x00000040

/* Bitmap Codec: codecGUID (2.2.7.2.10.1.1) */

/* CODEC_GUID_NSCODEC  CA8D1BB9-000F-154F-589F-AE2D1A87E2D6 */
#define XR_CODEC_GUID_NSCODEC \
    "\xb9\x1b\x8d\xca\x0f\x00\x4f\x15\x58\x9f\xae\x2d\x1a\x87\xe2\xd6"

/* CODEC_GUID_REMOTEFX 76772F12-BD72-4463-AFB3-B73C9C6F7886 */
#define XR_CODEC_GUID_REMOTEFX \
    "\x12\x2F\x77\x76\x72\xBD\x63\x44\xAF\xB3\xB7\x3C\x9C\x6F\x78\x86"

/* CODEC_GUID_IMAGE_REMOTEFX 2744CCD4-9D8A-4E74-803C-0ECBEEA19C54 */
#define XR_CODEC_GUID_IMAGE_REMOTEFX \
    "\xD4\xCC\x44\x27\x8A\x9D\x74\x4E\x80\x3C\x0E\xCB\xEE\xA1\x9C\x54"

/* MFVideoFormat_H264  34363248-0000-0010-8000-00AA00389B71 */
#define XR_CODEC_GUID_H264 \
    "\x48\x32\x36\x34\x00\x00\x10\x00\x80\x00\x00\xAA\x00\x38\x9B\x71"

/* CODEC_GUID_JPEG     1BAF4CE6-9EED-430C-869A-CB8B37B66237 */
#define XR_CODEC_GUID_JPEG \
    "\xE6\x4C\xAF\x1B\xED\x9E\x0C\x43\x86\x9A\xCB\x8B\x37\xB6\x62\x37"

/* CODEC_GUID_PNG      0E0C858D-28E0-45DB-ADAA-0F83E57CC560 */
#define XR_CODEC_GUID_PNG \
    "\x8D\x85\x0C\x0E\xE0\x28\xDB\x45\xAD\xAA\x0F\x83\xE5\x7C\xC5\x60"

/* CODEC_GUID_IGNORE   0C4351A6-3535-42AE-910C-CDFCE5760B58 */
#define XR_CODEC_GUID_IGNORE \
    "\xA6\x51\x43\x0C\x35\x35\xAE\x42\x91\x0C\xCD\xFC\xE5\x76\x0B\x58"

/* PDU Types (2.2.8.1.1.1.1) */
#define PDUTYPE_DEMANDACTIVEPDU        0x1
#define PDUTYPE_CONFIRMACTIVEPDU       0x3
#define PDUTYPE_DEACTIVATEALLPDU       0x6
#define PDUTYPE_DATAPDU                0x7
#define PDUTYPE_SERVER_REDIR_PKT       0xA

#define PDUTYPE_TO_STR(pdu_type) \
    ((pdu_type) == PDUTYPE_DEMANDACTIVEPDU ? "PDUTYPE_DEMANDACTIVEPDU" : \
     (pdu_type) == PDUTYPE_CONFIRMACTIVEPDU ? "PDUTYPE_CONFIRMACTIVEPDU" : \
     (pdu_type) == PDUTYPE_DEACTIVATEALLPDU ? "PDUTYPE_DEACTIVATEALLPDU" : \
     (pdu_type) == PDUTYPE_DATAPDU ? "PDUTYPE_DATAPDU" : \
     (pdu_type) == PDUTYPE_SERVER_REDIR_PKT ? "PDUTYPE_SERVER_REDIR_PKT" : \
     "unknown" \
    )

/* Share Data Header: pduType2 (2.2.8.1.1.1.2) */
/* TODO: to be renamed */
#define RDP_DATA_PDU_UPDATE            2   /* PDUTYPE2_UPDATE */
#define RDP_DATA_PDU_CONTROL           20
#define RDP_DATA_PDU_POINTER           27
#define RDP_DATA_PDU_INPUT             28
#define RDP_DATA_PDU_SYNCHRONISE       31
#define PDUTYPE2_REFRESH_RECT          33
#define RDP_DATA_PDU_PLAY_SOUND        34
#define PDUTYPE2_SUPPRESS_OUTPUT       35
#define PDUTYPE2_SHUTDOWN_REQUEST      36
#define PDUTYPE2_SHUTDOWN_DENIED       37
#define RDP_DATA_PDU_LOGON             38
#define RDP_DATA_PDU_FONT2             39
#define RDP_DATA_PDU_DISCONNECT        47
#define PDUTYPE2_MONITOR_LAYOUT_PDU    55

/* TS_SECURITY_HEADER: flags (2.2.8.1.1.2.1) */
/* TODO: to be renamed */
#define SEC_CLIENT_RANDOM              0x0001 /* SEC_EXCHANGE_PKT? */
#define SEC_ENCRYPT                    0x0008
#define SEC_LOGON_INFO                 0x0040 /* SEC_INFO_PKT */
#define SEC_LICENCE_NEG                0x0080 /* SEC_LICENSE_PKT */

#define SEC_TAG_SRV_INFO               0x0c01 /* SC_CORE */
#define SEC_TAG_SRV_CRYPT              0x0c02 /* SC_SECURITY */
#define SEC_TAG_SRV_CHANNELS           0x0c03 /* SC_NET? */

/* Slow-Path Input Event: messageType (2.2.8.1.1.3.1.1) */
/* TODO: to be renamed */
#define RDP_INPUT_SYNCHRONIZE          0
#define RDP_INPUT_CODEPOINT            1
#define RDP_INPUT_VIRTKEY              2
#define RDP_INPUT_SCANCODE             4
#define RDP_INPUT_UNICODE              5
#define RDP_INPUT_MOUSE                0x8001
#define RDP_INPUT_MOUSEX               0x8002

/* Keyboard Event: keyboardFlags (2.2.8.1.1.3.1.1.1) */
/* TODO: to be renamed */
#define KBD_FLAG_RIGHT                 0x0001
#define KBD_FLAG_EXT                   0x0100 /* KBDFLAGS_EXTENDED */
#define KBD_FLAG_QUIET                 0x1000
#define KBD_FLAG_DOWN                  0x4000
#define KBD_FLAG_UP                    0x8000

/* Mouse Event: pointerFlags (2.2.8.1.1.3.1.1.3) */
#define PTRFLAGS_HWHEEL                0x0400
#define PTRFLAGS_WHEEL                 0x0200
#define PTRFLAGS_WHEEL_NEGATIVE        0x0100
#define WheelRotationMask              0x01FF
#define PTRFLAGS_MOVE                  0x0800
#define PTRFLAGS_DOWN                  0x8000
#define PTRFLAGS_BUTTON1               0x1000
#define PTRFLAGS_BUTTON2               0x2000
#define PTRFLAGS_BUTTON3               0x4000

/* Extended Mouse Event: pointerFlags (2.2.8.1.1.3.1.1.4) */
#define PTRXFLAGS_DOWN                 0x8000
#define PTRXFLAGS_BUTTON1              0x0001
#define PTRXFLAGS_BUTTON2              0x0002

/* Synchronize Event: toggleFlags (2.2.8.1.1.3.1.1.5) */
/* TODO: to be renamed */
#define KBD_FLAG_SCROLL                0x0001 /* TS_SYNC_SCROLL_LOCK */
#define KBD_FLAG_NUMLOCK               0x0002
#define KBD_FLAG_CAPITAL               0x0004
#define TS_SYNC_KANA_LOCK              0x0008

/* Client Fast-Path Input Event PDU 2.2.8.1.2 */
#define FASTPATH_INPUT_ENCRYPTED            0x2

/* Fast-Path Input Event: eventCode (2.2.8.1.2.2) */
#define FASTPATH_INPUT_EVENT_SCANCODE       0x0
#define FASTPATH_INPUT_EVENT_MOUSE          0x1
#define FASTPATH_INPUT_EVENT_MOUSEX         0x2
#define FASTPATH_INPUT_EVENT_SYNC           0x3
#define FASTPATH_INPUT_EVENT_UNICODE        0x4
#define FASTPATH_INPUT_EVENT_QOE_TIMESTAMP  0x6

/* Fast-Path Keyboard Event: eventHeader (2.2.8.1.2.2.1) */
#define FASTPATH_INPUT_KBDFLAGS_RELEASE     0x01
#define FASTPATH_INPUT_KBDFLAGS_EXTENDED    0x02
#define FASTPATH_INPUT_KBDFLAGS_EXTENDED1   0x04

/* Slow-Path Graphics Update: updateType (2.2.9.1.1.3.1) */
/* TODO: to be renamed */
#define RDP_UPDATE_ORDERS              0
#define RDP_UPDATE_BITMAP              1
#define RDP_UPDATE_PALETTE             2
#define RDP_UPDATE_SYNCHRONIZE         3

#define GRAPHICS_UPDATE_TYPE_TO_STR(type) \
    ((type) == RDP_UPDATE_ORDERS ? "RDP_UPDATE_ORDERS" : \
     (type) == RDP_UPDATE_BITMAP ? "RDP_UPDATE_BITMAP" : \
     (type) == RDP_UPDATE_PALETTE ? "RDP_UPDATE_PALETTE" : \
     (type) == RDP_UPDATE_SYNCHRONIZE ? "RDP_UPDATE_SYNCHRONIZE" : \
     "unknown" \
    )

/* Server Pointer Update PDU: messageType (2.2.9.1.1.4) */
/* TODO: to be renamed */
#define RDP_POINTER_SYSTEM             1 /* TS_PTRMSGTYPE_SYSTEM */
#define RDP_POINTER_MOVE               3
#define RDP_POINTER_COLOR              6
#define RDP_POINTER_CACHED             7
#define RDP_POINTER_POINTER            8

/* System Pointer Update: systemPointerType (2.2.9.1.1.4.3) */
#define RDP_NULL_POINTER               0
#define RDP_DEFAULT_POINTER            0x7F00

/* Server Fast-Path Update PDU: action (2.2.9.1.2) */
#define FASTPATH_OUTPUT_ACTION_FASTPATH     0x0
#define FASTPATH_OUTPUT_ACTION_X224         0x3

/* Server Fast-Path Update PDU: flags (2.2.9.1.2) */
#define FASTPATH_OUTPUT_SECURE_CHECKSUM     0x1
#define FASTPATH_OUTPUT_ENCRYPTED           0x2

/* Fast-Path Update: updateCode (2.2.9.1.2.1) */
#define FASTPATH_UPDATETYPE_ORDERS        0x0
#define FASTPATH_UPDATETYPE_BITMAP        0x1
#define FASTPATH_UPDATETYPE_PALETTE       0x2
#define FASTPATH_UPDATETYPE_SYNCHRONIZE   0x3
#define FASTPATH_UPDATETYPE_SURFCMDS      0x4
#define FASTPATH_UPDATETYPE_PTR_NULL      0x5
#define FASTPATH_UPDATETYPE_PTR_DEFAULT   0x6
#define FASTPATH_UPDATETYPE_PTR_POSITION  0x8
#define FASTPATH_UPDATETYPE_COLOR         0x9
#define FASTPATH_UPDATETYPE_CACHED        0xA
#define FASTPATH_UPDATETYPE_POINTER       0xB

/* Fast-Path Update: fragmentation (2.2.9.1.2.1) */
#define FASTPATH_FRAGMENT_SINGLE          0x0
#define FASTPATH_FRAGMENT_LAST            0x1
#define FASTPATH_FRAGMENT_FIRST           0x2
#define FASTPATH_FRAGMENT_NEXT            0x3
#define FASTPATH_OUTPUT_COMPRESSION_USED  0x2

/* Surface Command Type (2.2.9.1.2.1.10.1) */
#define CMDTYPE_SET_SURFACE_BITS       0x0001
#define CMDTYPE_FRAME_MARKER           0x0004
#define CMDTYPE_STREAM_SURFACE_BITS    0x0006

/* Compression Flags (3.1.8.2.1) */
/* TODO: to be renamed, not used anywhere */
#define RDP_MPPC_COMPRESSED            0x20
#define RDP_MPPC_RESET                 0x40
#define RDP_MPPC_FLUSH                 0x80
#define RDP_MPPC_DICT_SIZE             8192 /* RDP 4.0 | MS-RDPBCGR 3.1.8 */

/* largePointerSupprtFlags (2.2.7.2.7) */
#define LARGE_POINTER_FLAG_96x96   0x00000001
#define LARGE_POINTER_FLAG_384x384 0x00000002

#endif /* MS_RDPBCGR_H */
