/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * MS-SMB2 : Definitions from [MS-SMB2]
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
 * References to MS-SMB2 are currently correct for v20190923 of that
 * document
 */

#if !defined(MS_SMB2_H)
#define MS_SMB2_H

/* SMB2 CREATE request values (section 2.2.13) */

/*
 * ShareAccess Mask. Currently, this is referred
 * to in MS-RDPEFS 2.2.1.4.1 as 'SharedAccess' rather than 'ShareAccess'.
 */
#define SA_FILE_SHARE_READ              0x00000001
#define SA_FILE_SHARE_WRITE             0x00000002
#define SA_FILE_SHARE_DELETE            0x00000004

/* CreateDisposition Mask */
#define CD_FILE_SUPERSEDE               0x00000000
#define CD_FILE_OPEN                    0x00000001
#define CD_FILE_CREATE                  0x00000002
#define CD_FILE_OPEN_IF                 0x00000003
#define CD_FILE_OVERWRITE               0x00000004
#define CD_FILE_OVERWRITE_IF            0x00000005

/* CreateOptions Mask */
enum CREATE_OPTIONS
{
    CO_FILE_DIRECTORY_FILE          = 0x00000001,
    CO_FILE_WRITE_THROUGH           = 0x00000002,
    CO_FILE_SYNCHRONOUS_IO_NONALERT = 0x00000020,
    CO_FILE_DELETE_ON_CLOSE         = 0x00001000
};

/*
 * DesiredAccess Mask (section 2.2.13.1.1)
 */

#define DA_FILE_READ_DATA               0x00000001
#define DA_FILE_WRITE_DATA              0x00000002
#define DA_FILE_APPEND_DATA             0x00000004
#define DA_FILE_READ_EA                 0x00000008 /* rd extended attributes */
#define DA_FILE_WRITE_EA                0x00000010 /* wr extended attributes */
#define DA_FILE_EXECUTE                 0x00000020
#define DA_FILE_READ_ATTRIBUTES         0x00000080
#define DA_FILE_WRITE_ATTRIBUTES        0x00000100
#define DA_DELETE                       0x00010000
#define DA_READ_CONTROL                 0x00020000 /* rd security descriptor */
#define DA_WRITE_DAC                    0x00040000
#define DA_WRITE_OWNER                  0x00080000
#define DA_SYNCHRONIZE                  0x00100000
#define DA_ACCESS_SYSTEM_SECURITY       0x01000000
#define DA_MAXIMUM_ALLOWED              0x02000000
#define DA_GENERIC_ALL                  0x10000000
#define DA_GENERIC_EXECUTE              0x20000000
#define DA_GENERIC_WRITE                0x40000000
#define DA_GENERIC_READ                 0x80000000

#endif /* MS_SMB2_H */



