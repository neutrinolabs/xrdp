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
 * Constants defined in publically available Microsoft documents are not
 * stored here, but are stored in the include files ms-*.h, where the name
 * of the file is the name of the document defining the constant.
 *
 * So for example, NTSTATUS values found in [MS-ERREF] are found in
 * ms-erref.h
 ******************************************************************************/

#define INFO_CLIENT_NAME_BYTES  32
/**
 * Maximum length of a string including the mandatory null terminator
 * [MS-RDPBCGR] TS_INFO_PACKET(2.2.1.11.1.1)
 */
#define INFO_CLIENT_MAX_CB_LEN  512

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
/*
 * not yet sorted out
 */

#define MCS_CONNECT_INITIAL            0x7f65 /* MCS BER: big endian, class=application (0x4000), constructed (0x2000), tag number > 30 (0x1f00), tag number=101 (0x0065) */
#define MCS_CONNECT_RESPONSE           0x7f66 /* MCS BER: application 102 */

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
#define WM_BUTTON8UP   115
#define WM_BUTTON8DOWN 116
#define WM_BUTTON9UP   117
#define WM_BUTTON9DOWN 118
#define WM_INVALIDATE  200

#define CB_ITEMCHANGE  300

#define FASTPATH_MAX_PACKET_SIZE    0x3fff

#define XR_RDP_SCAN_LSHIFT 42
#define XR_RDP_SCAN_ALT    56

#endif
