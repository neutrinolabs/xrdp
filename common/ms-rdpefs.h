/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * MS-RDPEFS : Definitions from [MS-RDPEFS]
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
 * References to MS-RDPEFS are currently correct for v20180912 of that
 * document
 */

#if !defined(MS_RDPEFS_H)
#define MS_RDPEFS_H

/*
 * RDPDR_HEADER definitions (2.2.1.1)
 */

/* device redirector core component; most of the packets in this protocol */
/* are sent under this component ID                                       */
#define RDPDR_CTYP_CORE                 0x4472

/* printing component. the packets that use this ID are typically about   */
/* printer cache management and identifying XPS printers                  */
#define RDPDR_CTYP_PRN                  0x5052

/* Server Announce Request, as specified in section 2.2.2.2               */
#define PAKID_CORE_SERVER_ANNOUNCE      0x496E

/* Client Announce Reply and Server Client ID Confirm, as specified in    */
/* sections 2.2.2.3 and 2.2.2.6.                                          */
#define PAKID_CORE_CLIENTID_CONFIRM     0x4343

/* Client Name Request, as specified in section 2.2.2.4                   */
#define PAKID_CORE_CLIENT_NAME          0x434E

/* Client Device List Announce Request, as specified in section 2.2.2.9   */
#define PAKID_CORE_DEVICELIST_ANNOUNCE  0x4441

/* Server Device Announce Response, as specified in section 2.2.2.1       */
#define PAKID_CORE_DEVICE_REPLY         0x6472

/* Device I/O Request, as specified in section 2.2.1.4                    */
#define PAKID_CORE_DEVICE_IOREQUEST     0x4952

/* Device I/O Response, as specified in section 2.2.1.5                   */
#define PAKID_CORE_DEVICE_IOCOMPLETION  0x4943

/* Server Core Capability Request, as specified in section 2.2.2.7        */
#define PAKID_CORE_SERVER_CAPABILITY    0x5350

/* Client Core Capability Response, as specified in section 2.2.2.8       */
#define PAKID_CORE_CLIENT_CAPABILITY    0x4350

/* Client Drive Device List Remove, as specified in section 2.2.3.2       */
#define PAKID_CORE_DEVICELIST_REMOVE    0x444D

/* Add Printer Cachedata, as specified in [MS-RDPEPC] section 2.2.2.3     */
#define PAKID_PRN_CACHE_DATA            0x5043

/* Server User Logged On, as specified in section 2.2.2.5                 */
#define PAKID_CORE_USER_LOGGEDON        0x554C

/* Server Printer Set XPS Mode, as specified in [MS-RDPEPC] section 2.2.2.2 */
#define PAKID_PRN_USING_XPS             0x5543

/*
 * Capability header definitions (2.2.1.2)
 */

#define CAP_GENERAL_TYPE   0x0001 /* General cap set - GENERAL_CAPS_SET      */
#define CAP_PRINTER_TYPE   0x0002 /* Print cap set - PRINTER_CAPS_SET        */
#define CAP_PORT_TYPE      0x0003 /* Port cap set - PORT_CAPS_SET            */
#define CAP_DRIVE_TYPE     0x0004 /* Drive cap set - DRIVE_CAPS_SET          */
#define CAP_SMARTCARD_TYPE 0x0005 /* Smart card cap set - SMARTCARD_CAPS_SET */

/*
 * Device announce header (2.2.1.3)
 */
#define RDPDR_DTYP_SERIAL               0x0001
#define RDPDR_DTYP_PARALLEL             0x0002
#define RDPDR_DTYP_PRINT                0x0004
#define RDPDR_DTYP_FILESYSTEM           0x0008
#define RDPDR_DTYP_SMARTCARD            0x0020

/* Device I/O Request definitions (2.2.1.4) */
/* MajorFunction */
enum IRP_MJ
{
    IRP_MJ_CREATE                   = 0x00000000,
    IRP_MJ_CLOSE                    = 0x00000002,
    IRP_MJ_READ                     = 0x00000003,
    IRP_MJ_WRITE                    = 0x00000004,
    IRP_MJ_DEVICE_CONTROL           = 0x0000000E,
    IRP_MJ_QUERY_VOLUME_INFORMATION = 0x0000000A,
    IRP_MJ_SET_VOLUME_INFORMATION   = 0x0000000B,
    IRP_MJ_QUERY_INFORMATION        = 0x00000005,
    IRP_MJ_SET_INFORMATION          = 0x00000006,
    IRP_MJ_DIRECTORY_CONTROL        = 0x0000000C,
    IRP_MJ_LOCK_CONTROL             = 0x00000011
};

/* MinorFunction */
/* Set to zero unless MajorFunction code == IRP_MJ_DIRECTORY_CONTROL */
enum IRP_MN
{
    IRP_MN_NONE                     = 0x00000000, /* Name not in MS docs */
    IRP_MN_QUERY_DIRECTORY          = 0x00000001,
    IRP_MN_NOTIFY_CHANGE_DIRECTORY  = 0x00000002
};

/*
 * General Capability Set (2.2.2.7.1)
 */
/* extendedPDU fields */
#define RDPDR_USER_LOGGEDON_PDU         0x00000004

#endif /* MS_RDPEFS_H */
