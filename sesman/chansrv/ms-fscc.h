/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * MS-FSCC : Definitions from [MS-FSCC]
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
 * References to MS-FSCC are currently correct for v20190923 of that
 * document
 */

#if !defined(MS_FSCC_H)
#define MS_FSCC_H

/*
 * File system ioctl codes (section 2.3)
 */
#define FSCTL_DELETE_OBJECT_ID          0x900a0

/*
 * File information classes (section 2.4)
 */
enum FS_INFORMATION_CLASS
{
    FileAllocationInformation    = 19,  /* Set */
    FileBasicInformation         = 4,  /* Query, Set */
    FileBothDirectoryInformation = 3,  /* Query */
    FileDirectoryInformation     = 1,  /* Query */
    FileDispositionInformation   = 13, /* Set */
    FileEndOfFileInformation     = 20, /* Set */
    FileFullDirectoryInformation = 2,  /* Query */
    FileNamesInformation         = 12, /* Query */
    FileRenameInformation        = 10, /* Set */
    FileStandardInformation      = 5   /* Query */
};

/*
 * Size of structs above without trailing RESERVED fields (MS-RDPEFS
 * 2.2.3.3.8)
 */
#define FILE_BASIC_INFORMATION_SIZE 36
#define FILE_STD_INFORMATION_SIZE 22
#define FILE_END_OF_FILE_INFORMATION_SIZE 8

/* Windows file attributes (section 2.6) */
#define W_FILE_ATTRIBUTE_DIRECTORY      0x00000010
#define W_FILE_ATTRIBUTE_READONLY       0x00000001
#define W_FILE_ATTRIBUTE_SYSTEM         0x00000004
#define W_FILE_ATTRIBUTE_NORMAL         0x00000080

#endif /* MS_FSCC_H */



