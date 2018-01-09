/**
 * xrdp: A Remote Desktop Protocol server.
 * Miscellaneous protocol constants
 *
 * Copyright (C) Matthew Chapman 1999-2008
 * Copyright (C) Jay Sorg 2004-2014
 * Copyright (C) Kevin Zhou 2012
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
 */

#if !defined(XRDP_CONSTANTS_H)
#define XRDP_CONSTANTS_H

/* TCP port for Remote Desktop Protocol */
#define TCP_PORT_RDP                   3389

/******************************************************************************
 *
 * xrdp constants
 *
 ******************************************************************************/

#define INFO_CLIENT_NAME_BYTES  32

#define XRDP_MAX_BITMAP_CACHE_ID  3
#define XRDP_MAX_BITMAP_CACHE_IDX 2000
#define XRDP_BITMAP_CACHE_ENTRIES 2048

#define XR_MIN_KEY_CODE 8
#define XR_MAX_KEY_CODE 256

/*
 * Constants come from ITU-T Recommendations
 */

#define ISO_PDU_CR                     0xE0 /* X.224 Connection Request */
#define ISO_PDU_CC                     0xD0 /* X.224 Connection Confirm */
#define ISO_PDU_DR                     0x80 /* Disconnect Request */
#define ISO_PDU_DT                     0xF0 /* Data */
#define ISO_PDU_ER                     0x70 /* Error */

/* MCS PDU codes (T.125) */
#define MCS_EDRQ                       1  /* Erect Domain Request */
#define MCS_DPUM                       8  /* Disconnect Provider Ultimatum */
#define MCS_AURQ                       10 /* Attach User Request */
#define MCS_AUCF                       11 /* Attach User Confirm */
#define MCS_CJRQ                       14 /* Channel Join Request */
#define MCS_CJCF                       15 /* Channel Join Confirm */
#define MCS_SDRQ                       25 /* Send Data Request */
#define MCS_SDIN                       26 /* Send Data Indication */

/*
 * Constants come from Remote Desktop Protocol
 */

/* RDP Security Negotiation codes */
#define RDP_NEG_REQ                    0x01  /* MS-RDPBCGR 2.2.1.1.1 */
#define RDP_NEG_RSP                    0x02  /* MS-RDPBCGR 2.2.1.2.1 */
#define RDP_NEG_FAILURE                0x03  /* MS-RDPBCGR 2.2.1.2.2 */
#define RDP_CORRELATION_INFO           0x06  /* MS-RDPBCGR 2.2.1.1.2 */

/* Protocol types codes (MS-RDPBCGR 2.2.1.1.1) */
#define PROTOCOL_RDP                   0x00000000
#define PROTOCOL_SSL                   0x00000001
#define PROTOCOL_HYBRID                0x00000002
#define PROTOCOL_RDSTLS                0x00000004
#define PROTOCOL_HYBRID_EX             0x00000008

/* Negotiation packet flags (MS-RDPBCGR 2.2.1.2.1) */
#define EXTENDED_CLIENT_DATA_SUPPORTED            0x01
#define DYNVC_GFX_PROTOCOL_SUPPORTED              0x02
#define NEGRSP_RESERVED                           0x04
#define RESTRICTED_ADMIN_MODE_SUPPORTED           0x08
#define REDIRECTED_AUTHENTICATION_MODE_SUPPORTED  0x10

/* RDP Negotiation Failure Codes (MS-RDPBCGR 2.2.1.2.2) */
#define SSL_REQUIRED_BY_SERVER                 0x00000001
#define SSL_NOT_ALLOWED_BY_SERVER              0x00000002
#define SSL_CERT_NOT_ON_SERVER                 0x00000003
#define INCONSISTENT_FLAGS                     0x00000004
#define HYBRID_REQUIRED_BY_SERVER              0x00000005
#define SSL_WITH_USER_AUTH_REQUIRED_BY_SERVER  0x00000006

/* Client Core Data: connectionType  (MS-RDPBCGR 2.2.1.3.2) */
#define CONNECTION_TYPE_MODEM          0x01
#define CONNECTION_TYPE_BROADBAND_LOW  0x02
#define CONNECTION_TYPE_SATELLITE      0x03
#define CONNECTION_TYPE_BROADBAND_HIGH 0x04
#define CONNECTION_TYPE_WAN            0x05
#define CONNECTION_TYPE_LAN            0x06
#define CONNECTION_TYPE_AUTODETECT     0x07

/* Client Core Data: colorDepth, postBeta2ColorDepth (MS-RDPBCGR 2.2.1.3.2) */
#define RNS_UD_COLOR_4BPP              0xCA00
#define RNS_UD_COLOR_8BPP              0xCA01
#define RNS_UD_COLOR_16BPP_555         0xCA02
#define RNS_UD_COLOR_16BPP_565         0xCA03
#define RNS_UD_COLOR_24BPP             0xCA04

/* Slow-Path Input Event: messageType (MS-RDPBCGR 2.2.8.1.1.3.1.1) */
/* TODO: to be renamed */
#define RDP_INPUT_SYNCHRONIZE          0
#define RDP_INPUT_CODEPOINT            1
#define RDP_INPUT_VIRTKEY              2
#define RDP_INPUT_SCANCODE             4
#define RDP_INPUT_UNICODE              5
#define RDP_INPUT_MOUSE                0x8001
#define RDP_INPUT_MOUSEX               0x8002

#define FASTPATH_INPUT_ENCRYPTED            0x2

/* Fast-Path Input Event: eventCode (MS-RDPBCGR 2.2.8.1.2.2) */
#define FASTPATH_INPUT_EVENT_SCANCODE       0x0
#define FASTPATH_INPUT_EVENT_MOUSE          0x1
#define FASTPATH_INPUT_EVENT_MOUSEX         0x2
#define FASTPATH_INPUT_EVENT_SYNC           0x3
#define FASTPATH_INPUT_EVENT_UNICODE        0x4
#define FASTPATH_INPUT_EVENT_QOE_TIMESTAMP  0x6

/* Fast-Path Keyboard Event: eventHeader (MS-RDPBCGR 2.2.8.2.2.1) */
#define FASTPATH_INPUT_KBDFLAGS_RELEASE     0x01
#define FASTPATH_INPUT_KBDFLAGS_EXTENDED    0x02
#define FASTPATH_INPUT_KBDFLAGS_EXTENDED1   0x04

/* Server Fast-Path Update PDU: action (MS-RDPBCGR 2.2.0.1.2) */
#define FASTPATH_OUTPUT_ACTION_FASTPATH     0x0
#define FASTPATH_OUTPUT_ACTION_X224         0x3

/* Server Fast-Path Update PDU: flags (MS-RDPBCGR 2.2.0.1.2) */
#define FASTPATH_OUTPUT_ACTION_FASTPATH     0x0
#define FASTPATH_OUTPUT_SECURE_CHECKSUM     0x1
#define FASTPATH_OUTPUT_ENCRYPTED           0x2

/* Fast-Path Update: updateCode (MS-RDPBCGR 2.2.9.1.2.1) */
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

/* Fast-Path Update: fragmentation (MS-RDPBCGR 2.2.9.1.2.1) */
#define FASTPATH_FRAGMENT_SINGLE          0x0
#define FASTPATH_FRAGMENT_LAST            0x1
#define FASTPATH_FRAGMENT_FIRST           0x2
#define FASTPATH_FRAGMENT_NEXT            0x3
#define FASTPATH_OUTPUT_COMPRESSION_USED  0x2

/* Mouse Event: pointerFlags (MS-RDPBCGR 2.2.8.1.1.3.1.1.3) */
#define PTRFLAGS_HWHEEL                0x0400
#define PTRFLAGS_WHEEL                 0x0200
#define PTRFLAGS_WHEEL_NEGATIVE        0x0100
#define WheelRotationMask              0x01FF
#define PTRFLAGS_MOVE                  0x0800
#define PTRFLAGS_DOWN                  0x8000
#define PTRFLAGS_BUTTON1               0x1000
#define PTRFLAGS_BUTTON2               0x2000
#define PTRFLAGS_BUTTON3               0x4000

/* Extended Mouse Event: pointerFlags (MS-RDPBCGR 2.2.8.1.1.3.1.1.4) */
#define PTRXFLAGS_DOWN                 0x8000
#define PTRXFLAGS_BUTTON1              0x0001
#define PTRXFLAGS_BUTTON2              0x0002

/* General Capability Set: osMajorType (MS-RDPBCGR 2.2.7.1.1) */
#define OSMAJORTYPE_UNSPECIFIED        0x0000
#define OSMAJORTYPE_WINDOWS            0x0001
#define OSMAJORTYPE_OS2                0x0002
#define OSMAJORTYPE_MACINTOSH          0x0003
#define OSMAJORTYPE_UNIX               0x0004
#define OSMAJORTYPE_IOS                0x0005
#define OSMAJORTYPE_OSX                0x0006
#define OSMAJORTYPE_ANDROID            0x0007
#define OSMAJORTYPE_CHROME_OS          0x0008

/* General Capability Set: osMinorType (MS-RDPBCGR 2.2.7.1.1) */
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

/* Extended Info Packet: performanceFlags (MS-RDPBCGR 2.2.1.11.1.1.1) */
/* TODO: to be renamed */
#define RDP5_DISABLE_NOTHING           0x00
#define RDP5_NO_WALLPAPER              0x01
#define RDP5_NO_FULLWINDOWDRAG         0x02
#define RDP5_NO_MENUANIMATIONS         0x04
#define RDP5_NO_THEMING                0x08
#define RDP5_NO_CURSOR_SHADOW          0x20
#define RDP5_NO_CURSORSETTINGS         0x40 /* disables cursor blinking */

/* Virtual channel options */
/* Channel Definition Structure: options (MS-RDPBCGR 2.2.1.3.4.1) */
#define REMOTE_CONTROL_PERSISTENT       0x00100000
/* NOTE: XR_ prefixed to avoid conflict with FreeRDP */
#define XR_CHANNEL_OPTION_SHOW_PROTOCOL 0x00200000
#define XR_CHANNEL_OPTION_COMPRESS      0x00400000
#define XR_CHANNEL_OPTION_COMPRESS_RDP  0x00800000
#define XR_CHANNEL_OPTION_PRI_LOW       0x02000000
#define XR_CHANNEL_OPTION_PRI_MED       0x04000000
#define XR_CHANNEL_OPTION_PRI_HIGH      0x08000000
#define XR_CHANNEL_OPTION_ENCRYPT_CS    0x10000000
#define XR_CHANNEL_OPTION_ENCRYPT_SC    0x20000000
#define XR_CHANNEL_OPTION_ENCRYPT_RDP   0x40000000
#define XR_CHANNEL_OPTION_INITIALIZED   0x80000000

/* RDPDR: Device Announce Header: DeviceType (MS-RDPEFS 2.2.1.3) */
/* TODO: to be renamed */
#define DEVICE_TYPE_SERIAL             0x01
#define DEVICE_TYPE_PARALLEL           0x02
#define DEVICE_TYPE_PRINTER            0x04
#define DEVICE_TYPE_DISK               0x08
#define DEVICE_TYPE_SCARD              0x20

/* Order Capability Set: orderSupportExFlags (MS-RDPBCGR 2.2.7.1.3)  */
#define XR_ORDERFLAGS_EX_CACHE_BITMAP_REV3_SUPPORT   0x0002
#define XR_ORDERFLAGS_EX_ALTSEC_FRAME_MARKER_SUPPORT 0x0004
#define XR_ORDERFLAGS_EX_OFFSCREEN_COMPOSITE_SUPPORT 0x0100

/* Order Capability Set: orderSupport (MS-RDPBCGR 2.2.7.1.3) */
#define TS_NEG_DSTBLT_INDEX             0x00
#define TS_NEG_PATBLT_INDEX             0x01
#define TS_NEG_SCRBLT_INDEX             0x02
#define TS_NEG_MEMBLT_INDEX             0x03
#define TS_NEG_MEM3BLT_INDEX            0x04
                                     /* 0x05 */
                                     /* 0x06 */
#define TS_NEG_DRAWNINEGRID_INDEX       0x07
#define TS_NEG_LINETO_INDEX             0x08
#define TS_NEG_MULTI_DRAWNINEGRID_INDEX 0x09
                                     /* 0x0A */
#define TS_NEG_SAVEBITMAP_INDEX         0x0B
                                     /* 0x0C */
                                     /* 0x0D */
                                     /* 0x0E */
#define TS_NEG_MULTIDSTBLT_INDEX        0x0F
#define TS_NEG_MULTIPATBLT_INDEX        0x10
#define TS_NEG_MULTISCRBLT_INDEX        0x11
#define TS_NEG_MULTIOPAQUERECT_INDEX    0x12
#define TS_NEG_FAST_INDEX_INDEX         0x13
#define TS_NEG_POLYGON_SC_INDEX         0x14
#define TS_NEG_POLYGON_CB_INDEX         0x15
#define TS_NEG_POLYLINE_INDEX           0x16
                                     /* 0x17 */
#define TS_NEG_FAST_GLYPH_INDEX         0x18
#define TS_NEG_ELLIPSE_SC_INDEX         0x19
#define TS_NEG_ELLIPSE_CB_INDEX         0x1A
#define TS_NEG_INDEX_INDEX              0x1B
                                     /* 0x1C */
                                     /* 0x1D */
                                     /* 0x1E */
                                     /* 0x1F */

/* General Capability Set: extraFlags (MS-RDPBCGR 2.2.7.1.1) */
#define  TS_CAPS_PROTOCOLVERSION       0x0200
#define  FASTPATH_OUTPUT_SUPPORTED     0x0001
#define  NO_BITMAP_COMPRESSION_HDR     0x0400
#define  LONG_CREDENTIALS_SUPPORTED    0x0004
#define  AUTORECONNECT_SUPPORTED       0x0008
#define  ENC_SALTED_CHECKSUM           0x0010

/* Order Capability Set: orderFlags (MS-RDPBCGR 2.2.7.1.3) */
#define  NEGOTIATEORDERSUPPORT         0x0002
#define  ZEROBOUNDSDELTASUPPORT        0x0008
#define  COLORINDEXSUPPORT             0x0020
#define  SOLIDPATTERNBRUSHONLY         0x0040
#define  ORDERFLAGS_EXTRA_FLAGS        0x0080

/* Input Capability Set: inputFlags (MS-RDPBCGR 2.2.7.1.6) */
#define  INPUT_FLAG_SCANCODES          0x0001
#define  INPUT_FLAG_MOUSEX             0x0004
#define  INPUT_FLAG_FASTPATH_INPUT     0x0008
#define  INPUT_FLAG_UNICODE            0x0010
#define  INPUT_FLAG_FASTPATH_INPUT2    0x0020
#define  INPUT_FLAG_UNUSED1            0x0040
#define  INPUT_FLAG_UNUSED2            0x0080
#define  TS_INPUT_FLAG_MOUSE_HWHEEL    0x0100
#define  TS_INPUT_FLAG_QOE_TIMESTAMPS  0x0200

/* Desktop Composition Capability Set: CompDeskSupportLevel (MS-RDPBCGR 2.2.7.2.8) */
#define  COMPDESK_NOT_SUPPORTED      0x0000
#define  COMPDESK_SUPPORTED          0x0001

/* Surface Commands Capability Set: cmdFlags (MS-RDPBCGR 2.2.7.2.9) */
#define SURFCMDS_SETSURFACEBITS      0x00000002
#define SURFCMDS_FRAMEMARKER         0x00000010
#define SURFCMDS_STREAMSUFRACEBITS   0x00000040

/* Bitmap Codec: codecGUID (MS-RDPBCGR 2.2.7.2.10.1.1) */

/* CODEC_GUID_NSCODEC  CA8D1BB9-000F-154F-589FAE2D1A87E2D6 */
#define XR_CODEC_GUID_NSCODEC \
  "\xb9\x1b\x8d\xca\x0f\x00\x4f\x15\x58\x9f\xae\x2d\x1a\x87\xe2\xd6"

/* CODEC_GUID_REMOTEFX 76772F12-BD72-4463-AFB3B73C9C6F7886 */
#define XR_CODEC_GUID_REMOTEFX \
  "\x12\x2F\x77\x76\x72\xBD\x63\x44\xAF\xB3\xB7\x3C\x9C\x6F\x78\x86"

/* CODEC_GUID_IMAGE_REMOTEFX 2744CCD4-9D8A-4E74-803C-0ECBEEA19C54 */
#define XR_CODEC_GUID_IMAGE_REMOTEFX \
  "\xD4\xCC\x44\x27\x8A\x9D\x74\x4E\x80\x3C\x0E\xCB\xEE\xA1\x9C\x54"

/* MFVideoFormat_H264  0x34363248-0000-0010-800000AA00389B71 */
#define XR_CODEC_GUID_H264 \
  "\x48\x32\x36\x34\x00\x00\x10\x00\x80\x00\x00\xAA\x00\x38\x9B\x71"

/* CODEC_GUID_JPEG     1BAF4CE6-9EED-430C-869ACB8B37B66237 */
#define XR_CODEC_GUID_JPEG \
  "\xE6\x4C\xAF\x1B\xED\x9E\x0C\x43\x86\x9A\xCB\x8B\x37\xB6\x62\x37"

/* CODEC_GUID_PNG      0E0C858D-28E0-45DB-ADAA0F83E57CC560 */
#define XR_CODEC_GUID_PNG \
  "\x8D\x85\x0C\x0E\xE0\x28\xDB\x45\xAD\xAA\x0F\x83\xE5\x7C\xC5\x60"

/* Surface Command Type (MS-RDPBCGR 2.2.9.1.2.1.10.1) */
#define CMDTYPE_SET_SURFACE_BITS       0x0001
#define CMDTYPE_FRAME_MARKER           0x0004
#define CMDTYPE_STREAM_SURFACE_BITS    0x0006

/* RDP5 disconnect PDU */
/* Set Error Info PDU Data: errorInfo (MS-RDPBCGR 2.2.5.1.1) */
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

/* Info Packet (TS_INFO_PACKET): flags (MS-RDPBCGR 2.2.1.11.1.1) */
/* TODO: to be renamed */
#define RDP_LOGON_AUTO                 0x0008
#define RDP_LOGON_NORMAL               0x0033
#define RDP_COMPRESSION                0x0080
#define RDP_LOGON_BLOB                 0x0100
#define RDP_LOGON_LEAVE_AUDIO          0x2000

/* Compression Flags (MS-RDPBCGR 3.1.8.2.1) */
/* TODO: to be renamed, not used anywhere */
#define RDP_MPPC_COMPRESSED            0x20
#define RDP_MPPC_RESET                 0x40
#define RDP_MPPC_FLUSH                 0x80
#define RDP_MPPC_DICT_SIZE             8192 /* RDP 4.0 | MS-RDPBCGR 3.1.8 */

/* Drawing Order: controlFlags (MS-RDPEGDI 2.2.2.2.1, ) */
/* TODO: to be renamed */
#define RDP_ORDER_STANDARD   0x01
#define RDP_ORDER_SECONDARY  0x02
#define RDP_ORDER_BOUNDS     0x04
#define RDP_ORDER_CHANGE     0x08
#define RDP_ORDER_DELTA      0x10
#define RDP_ORDER_LASTBOUNDS 0x20
#define RDP_ORDER_SMALL      0x40
#define RDP_ORDER_TINY       0x80

/* Drawing Order: orderType (MS-RDPEGDI 2.2.2.2.1.1.2) ? */
#define RDP_ORDER_DESTBLT   0
#define RDP_ORDER_PATBLT    1
#define RDP_ORDER_SCREENBLT 2
#define RDP_ORDER_LINE      9
#define RDP_ORDER_RECT      10
#define RDP_ORDER_DESKSAVE  11
#define RDP_ORDER_MEMBLT    13
#define RDP_ORDER_TRIBLT    14
#define RDP_ORDER_POLYLINE  22
#define RDP_ORDER_TEXT2     27
#define RDP_ORDER_COMPOSITE 37 /* 0x25 */

/* Secondary Drawing Order Header: orderType (MS-RDPEGDI 2.2.2.2.1.2.1.1) */
/* TODO: to be renamed */
#define RDP_ORDER_RAW_BMPCACHE  0
#define RDP_ORDER_COLCACHE      1
#define RDP_ORDER_BMPCACHE      2
#define RDP_ORDER_FONTCACHE     3
#define RDP_ORDER_RAW_BMPCACHE2 4
#define RDP_ORDER_BMPCACHE2     5
#define RDP_ORDER_BRUSHCACHE    7
#define RDP_ORDER_BMPCACHE3     8

/* Maps to generalCapabilitySet in T.128 page 138 */

/* Capability Set: capabilitySetType (MS-RDPBCGR 2.2.1.13.1.1.1) */
/* TODO: to be renamed */
#define RDP_CAPSET_GENERAL             0x0001
#define RDP_CAPLEN_GENERAL             0x18

#define RDP_CAPSET_BITMAP              0x0002
#define RDP_CAPLEN_BITMAP              0x1C

#define RDP_CAPSET_ORDER               0x0003
#define RDP_CAPLEN_ORDER               0x58
#define ORDER_CAP_NEGOTIATE            2
#define ORDER_CAP_NOSUPPORT            4

#define RDP_CAPSET_BMPCACHE            0x0004
#define RDP_CAPLEN_BMPCACHE            0x28

#define RDP_CAPSET_CONTROL             0x0005
#define RDP_CAPLEN_CONTROL             0x0C

#define RDP_CAPSET_ACTIVATE            0x0007
#define RDP_CAPLEN_ACTIVATE            0x0C

#define RDP_CAPSET_POINTER             0x0008
#define RDP_CAPLEN_POINTER             0x0a
#define RDP_CAPLEN_POINTER_MONO        0x08

#define RDP_CAPSET_SHARE               0x0009
#define RDP_CAPLEN_SHARE               0x08

#define RDP_CAPSET_COLCACHE            0x000A
#define RDP_CAPLEN_COLCACHE            0x08

#define RDP_CAPSET_SOUND               0x000C

#define RDP_CAPSET_INPUT               0x000D
#define RDP_CAPLEN_INPUT               0x58

#define RDP_CAPSET_FONT                0x000E
#define RDP_CAPLEN_FONT                0x04

#define RDP_CAPSET_BRUSHCACHE          0x000F
#define RDP_CAPLEN_BRUSHCACHE          0x08

#define RDP_CAPSET_GLYPHCACHE          0x0010
#define RDP_CAPSET_OFFSCREENCACHE      0x0011

#define RDP_CAPSET_BITMAP_OFFSCREEN    0x0012
#define RDP_CAPLEN_BITMAP_OFFSCREEN    0x08

#define RDP_CAPSET_BMPCACHE2           0x0013
#define RDP_CAPLEN_BMPCACHE2           0x28
#define BMPCACHE2_FLAG_PERSIST         ((long)1<<31)

#define RDP_CAPSET_VIRCHAN             0x0014
#define RDP_CAPLEN_VIRCHAN             0x08

#define RDP_CAPSET_DRAWNINEGRIDCACHE   0x0015
#define RDP_CAPSET_DRAWGDIPLUS         0x0016
#define RDP_CAPSET_RAIL                0x0017
#define RDP_CAPSET_WINDOW              0x0018

#define RDP_CAPSET_COMPDESK            0x0019
#define RDP_CAPLEN_COMPDESK            0x06

#define RDP_CAPSET_MULTIFRAGMENT       0x001A
#define RDP_CAPLEN_MULTIFRAGMENT       0x08

#define RDP_CAPSET_LPOINTER            0x001B
#define RDP_CAPLEN_LPOINTER            0x06

#define RDP_CAPSET_FRAME_ACKNOWLEDGE   0x001E
#define RDP_CAPLEN_FRAME_ACKNOWLEDGE   0x08

#define RDP_CAPSET_SURFCMDS            0x001C
#define RDP_CAPLEN_SURFCMDS            0x0C

#define RDP_CAPSET_BMPCODECS           0x001D
#define RDP_CAPLEN_BMPCODECS           0x1C



/* TS_SECURITY_HEADER: flags (MS-RDPBCGR 2.2.8.1.1.2.1) */
/* TODO: to be renamed */
#define SEC_CLIENT_RANDOM              0x0001 /* SEC_EXCHANGE_PKT? */
#define SEC_ENCRYPT                    0x0008
#define SEC_LOGON_INFO                 0x0040 /* SEC_INFO_PKT */
#define SEC_LICENCE_NEG                0x0080 /* SEC_LICENSE_PKT */

#define SEC_TAG_SRV_INFO               0x0c01 /* SC_CORE */
#define SEC_TAG_SRV_CRYPT              0x0c02 /* SC_SECURITY */
#define SEC_TAG_SRV_CHANNELS           0x0c03 /* SC_NET? */

/* TS_UD_HEADER: type (MS-RDPBCGR (2.2.1.3.1) */
/* TODO: to be renamed */
#define SEC_TAG_CLI_INFO               0xc001 /* CS_CORE? */
#define SEC_TAG_CLI_CRYPT              0xc002 /* CS_SECURITY? */
#define SEC_TAG_CLI_CHANNELS           0xc003 /* CS_CHANNELS? */
#define SEC_TAG_CLI_4                  0xc004 /* CS_CLUSTER? */
#define SEC_TAG_CLI_MONITOR            0xc005 /* CS_MONITOR */

/* Server Proprietary Certificate (MS-RDPBCGR 2.2.1.4.3.1.1) */
/* TODO: to be renamed */
#define SEC_TAG_PUBKEY                 0x0006 /* BB_RSA_KEY_BLOB */
#define SEC_TAG_KEYSIG                 0x0008 /* BB_SIGNATURE_KEY_BLOB */

/* LicensingMessage (MS-RDPELE 2.2.2) */
/* TODO: to be renamed */
#define LICENCE_TAG_DEMAND             0x01 /* LICNSE_REQUEST */
#define LICENCE_TAG_AUTHREQ            0x02 /* PLATFORM_CHALLENGE */
#define LICENCE_TAG_ISSUE              0x03 /* NEW_LICENSE */
#define LICENCE_TAG_REISSUE            0x04 /* UPGRADE_LICENSE */
#define LICENCE_TAG_PRESENT            0x12 /* LICENSE_INFO */
#define LICENCE_TAG_REQUEST            0x13 /* NEW_LICENSE_REQUEST */
#define LICENCE_TAG_AUTHRESP           0x15 /* PLATFORM_CHALLENGE_RESPONSE */
#define LICENCE_TAG_RESULT             0xff

/* LICENSE_BINARY_BLOB (MS-RDPBCGR 2.2.1.12.1.2) */
#define LICENCE_TAG_USER               0x000f /* BB_CLIENT_USER_NAME_BLOB */
#define LICENCE_TAG_HOST               0x0010 /* BB_CLIENT_MACHINE_NAME_BLOB */

/* Share Data Header: pduType2 (MS-RDPBCGR 2.2.8.1.1.1.2) */
/* TODO: to be renamed */
#define RDP_DATA_PDU_UPDATE            2   /* PDUTYPE2_UPDATE */
#define RDP_DATA_PDU_CONTROL           20
#define RDP_DATA_PDU_POINTER           27
#define RDP_DATA_PDU_INPUT             28
#define RDP_DATA_PDU_SYNCHRONISE       31
#define RDP_DATA_PDU_PLAY_SOUND        34
#define RDP_DATA_PDU_LOGON             38
#define RDP_DATA_PDU_FONT2             39
#define RDP_DATA_PDU_DISCONNECT        47

/* Control PDU Data: action (MS-RDPBCGR 2.2.1.15.1) */
/* TODO: to be renamed */
#define RDP_CTL_REQUEST_CONTROL        1 /* CTRLACTION_REQUEST_CONTROL */
#define RDP_CTL_GRANT_CONTROL          2
#define RDP_CTL_DETACH                 3
#define RDP_CTL_COOPERATE              4

/* Slow-Path Graphics Update: updateType (MS-RDPBCGR 2.2.9.1.1.3.1) */
/* TODO: to be renamed */
#define RDP_UPDATE_ORDERS              0
#define RDP_UPDATE_BITMAP              1
#define RDP_UPDATE_PALETTE             2
#define RDP_UPDATE_SYNCHRONIZE         3

/* Server Pointer Update PDU: messageType (MS-RDPBCGR 2.2.9.1.1.4) */
/* TODO: to be renamed */
#define RDP_POINTER_SYSTEM             1 /* TS_PTRMSGTYPE_SYSTEM */
#define RDP_POINTER_MOVE               3
#define RDP_POINTER_COLOR              6
#define RDP_POINTER_CACHED             7
#define RDP_POINTER_POINTER            8

/* System Pointer Update: systemPointerType (MS-RDPBCGR 2.2.9.1.1.4.3) */
#define RDP_NULL_POINTER               0
#define RDP_DEFAULT_POINTER            0x7F00

/* Keyboard Event: keyboardFlags (MS-RDPBCGR 2.2.8.1.1.3.1.1.1) */
/* TODO: to be renamed */
#define KBD_FLAG_RIGHT                 0x0001 
#define KBD_FLAG_EXT                   0x0100 /* KBDFLAGS_EXTENDED */
#define KBD_FLAG_QUIET                 0x1000
#define KBD_FLAG_DOWN                  0x4000
#define KBD_FLAG_UP                    0x8000

/* Synchronize Event: toggleFlags (MS-RDPBCGR 2.2.8.1.1.3.1.1.5) */
/* TODO: to be renamed */
#define KBD_FLAG_SCROLL                0x0001 /* TS_SYNC_SCROLL_LOCK */
#define KBD_FLAG_NUMLOCK               0x0002
#define KBD_FLAG_CAPITAL               0x0004
#define TS_SYNC_KANA_LOCK              0x0008

/******************************************************************************
 *
 * Constants come from other Microsoft products
 *
 *****************************************************************************/

/* Sound format constants - see also RFC 2361 and MS-RDPAI  */
#define WAVE_FORMAT_PCM                0x0001
#define WAVE_FORMAT_ADPCM              0x0002
#define WAVE_FORMAT_ALAW               0x0006
#define WAVE_FORMAT_MULAW              0x0007
#define WAVE_FORMAT_MPEGLAYER3         0x0055
#define WAVE_FORMAT_OPUS               0x0069
#define WAVE_FORMAT_AAC                0xA106

/* https://technet.microsoft.com/ja-jp/library/aa387685.aspx */
#define SEC_RSA_MAGIC                  0x31415352 /* RSA1 */

/* NTSTATUS Values (MS-ERREF 2.3.1) */
/* used for RDPDR */
#define STATUS_SUCCESS                 0x00000000
#define STATUS_PENDING                 0x00000103

#define STATUS_NO_MORE_FILES           0x80000006
#define STATUS_DEVICE_PAPER_EMPTY      0x8000000e
#define STATUS_DEVICE_POWERED_OFF      0x8000000f
#define STATUS_DEVICE_OFF_LINE         0x80000010
#define STATUS_DEVICE_BUSY             0x80000011

#define STATUS_INVALID_HANDLE          0xc0000008
#define STATUS_INVALID_PARAMETER       0xc000000d
#define STATUS_NO_SUCH_FILE            0xc000000f
#define STATUS_INVALID_DEVICE_REQUEST  0xc0000010
#define STATUS_ACCESS_DENIED           0xc0000022
#define STATUS_OBJECT_NAME_COLLISION   0xc0000035
#define STATUS_DISK_FULL               0xc000007f
#define STATUS_FILE_IS_A_DIRECTORY     0xc00000ba
#define STATUS_NOT_SUPPORTED           0xc00000bb
#define STATUS_TIMEOUT                 0xc0000102
#define STATUS_CANCELLED               0xc0000120

/* MS-SMB2 2.2.13 */
/* TODO: not used anywhere */
#define FILE_DIRECTORY_FILE            0x00000001
#define FILE_NON_DIRECTORY_FILE        0x00000040
#define FILE_OPEN_FOR_FREE_SPACE_QUERY 0x00800000

/*
 * not yet sorted out
 */

#define MCS_CONNECT_INITIAL            0x7f65
#define MCS_CONNECT_RESPONSE           0x7f66

#define BER_TAG_BOOLEAN                1
#define BER_TAG_INTEGER                2
#define BER_TAG_OCTET_STRING           4
#define BER_TAG_RESULT                 10
#define MCS_TAG_DOMAIN_PARAMS          0x30

#define MCS_GLOBAL_CHANNEL             1003
#define MCS_USERCHANNEL_BASE           1001

/* RDP secure transport constants */
/* not used anywhere */
#define SEC_RANDOM_SIZE                32
#define SEC_MODULUS_SIZE               64
#define SEC_PADDING_SIZE               8
#define SEC_EXPONENT_SIZE              4

/* RDP licensing constants */
#define LICENCE_TOKEN_SIZE             10
#define LICENCE_HWID_SIZE              20
#define LICENCE_SIGNATURE_SIZE         16


/* RDP PDU codes */
#define RDP_PDU_DEMAND_ACTIVE          1
#define RDP_PDU_CONFIRM_ACTIVE         3
#define RDP_PDU_REDIRECT               4
#define RDP_PDU_DEACTIVATE             6
#define RDP_PDU_DATA                   7

/* See T.128 */
/* not used anywhere */
#define RDP_KEYPRESS                   0
#define RDP_KEYRELEASE                 (KBD_FLAG_DOWN | KBD_FLAG_UP)

/* Raster operation masks */
#define ROP2_S(rop3)                   (rop3 & 0xf)
#define ROP2_P(rop3)                   ((rop3 & 0x3) | ((rop3 & 0x30) >> 2))

#define ROP2_COPY                      0xc
#define ROP2_XOR                       0x6
#define ROP2_AND                       0x8
#define ROP2_NXOR                      0x9
#define ROP2_OR                        0xe

#define MIX_TRANSPARENT                0
#define MIX_OPAQUE                     1

#define TEXT2_VERTICAL                 0x04
#define TEXT2_IMPLICIT_X               0x20

/* RDP bitmap cache (version 2) constants */
#define BMPCACHE2_C0_CELLS             0x78
#define BMPCACHE2_C1_CELLS             0x78
#define BMPCACHE2_C2_CELLS             0x150
#define BMPCACHE2_NUM_PSTCELLS         0x9f6

#define PDU_FLAG_FIRST                 0x01
#define PDU_FLAG_LAST                  0x02

#define RDP_SOURCE                     "MSTSC"

/* Keymap flags */
#define MapRightShiftMask              (1 << 0)
#define MapLeftShiftMask               (1 << 1)
#define MapShiftMask                   (MapRightShiftMask | MapLeftShiftMask)

#define MapRightAltMask                (1 << 2)
#define MapLeftAltMask                 (1 << 3)
#define MapAltGrMask                   MapRightAltMask

#define MapRightCtrlMask               (1 << 4)
#define MapLeftCtrlMask                (1 << 5)
#define MapCtrlMask                    (MapRightCtrlMask | MapLeftCtrlMask)

#define MapRightWinMask                (1 << 6)
#define MapLeftWinMask                 (1 << 7)
#define MapWinMask                     (MapRightWinMask | MapLeftWinMask)

#define MapNumLockMask                 (1 << 8)
#define MapCapsLockMask                (1 << 9)

#define MapLocalStateMask              (1 << 10)

#define MapInhibitMask                 (1 << 11)

#define MASK_ADD_BITS(var, mask)       (var |= mask)
#define MASK_REMOVE_BITS(var, mask)    (var &= ~mask)
#define MASK_HAS_BITS(var, mask)       ((var & mask)>0)
#define MASK_CHANGE_BIT(var, mask, active) \
                  (var = ((var & ~mask) | (active ? mask : 0)))

/* Clipboard constants, "borrowed" from GCC system headers in
   the w32 cross compiler */

#define CF_TEXT                        1
#define CF_BITMAP                      2
#define CF_METAFILEPICT                3
#define CF_SYLK                        4
#define CF_DIF                         5
#define CF_TIFF                        6
#define CF_OEMTEXT                     7
#define CF_DIB                         8
#define CF_PALETTE                     9
#define CF_PENDATA                     10
#define CF_RIFF                        11
#define CF_WAVE                        12
#define CF_UNICODETEXT                 13
#define CF_ENHMETAFILE                 14
#define CF_HDROP                       15
#define CF_LOCALE                      16
#define CF_MAX                         17
#define CF_OWNERDISPLAY                128
#define CF_DSPTEXT                     129
#define CF_DSPBITMAP                   130
#define CF_DSPMETAFILEPICT             131
#define CF_DSPENHMETAFILE              142
#define CF_PRIVATEFIRST                512
#define CF_PRIVATELAST                 767
#define CF_GDIOBJFIRST                 768
#define CF_GDIOBJLAST                  1023

/* RDPDR constants */
#define RDPDR_MAX_DEVICES              0x10

/* drawable types */
#define WND_TYPE_BITMAP  0
#define WND_TYPE_WND     1
#define WND_TYPE_SCREEN  2
#define WND_TYPE_BUTTON  3
#define WND_TYPE_IMAGE   4
#define WND_TYPE_EDIT    5
#define WND_TYPE_LABEL   6
#define WND_TYPE_COMBO   7
#define WND_TYPE_SPECIAL 8
#define WND_TYPE_LISTBOX 9
#define WND_TYPE_OFFSCREEN 10

/* button states */
#define BUTTON_STATE_UP   0
#define BUTTON_STATE_DOWN 1

/* messages */
#define WM_PAINT       3
#define WM_KEYDOWN     15
#define WM_KEYUP       16
#define WM_MOUSEMOVE   100
#define WM_LBUTTONUP   101
#define WM_LBUTTONDOWN 102
#define WM_RBUTTONUP   103
#define WM_RBUTTONDOWN 104
#define WM_BUTTON3UP   105
#define WM_BUTTON3DOWN 106
#define WM_BUTTON4UP   107
#define WM_BUTTON4DOWN 108
#define WM_BUTTON5UP   109
#define WM_BUTTON5DOWN 110
#define WM_BUTTON6UP   111
#define WM_BUTTON6DOWN 112
#define WM_BUTTON7UP   113
#define WM_BUTTON7DOWN 114
#define WM_INVALIDATE  200

#define CB_ITEMCHANGE  300

#define FASTPATH_MAX_PACKET_SIZE    0x3fff

#define XR_RDP_SCAN_LSHIFT 42
#define XR_RDP_SCAN_ALT    56

#endif
