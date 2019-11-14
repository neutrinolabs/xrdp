/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * MS-ERREF : Definitions from [MS-ERREF]
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
 * References to MS-ERREF are currently correct for v20180912 of that
 * document
 */

#if !defined(MS_ERREF_H)
#define MS_ERREF_H

/*
 * NTSTATUS codes (section 2.3)
 */
enum NTSTATUS
{
    NT_STATUS_SUCCESS               = 0x00000000,
    NT_STATUS_UNSUCCESSFUL          = 0xC0000001,
    NT_STATUS_NO_SUCH_FILE          = 0xC000000F,
    NT_STATUS_ACCESS_DENIED         = 0xC0000022,
    NT_STATUS_OBJECT_NAME_INVALID   = 0xC0000033,
    NT_STATUS_OBJECT_NAME_NOT_FOUND = 0xC0000034,
    NT_STATUS_SHARING_VIOLATION     = 0xC0000043,
    NT_STATUS_NO_MORE_FILES         = 0x80000006
};

#endif /* MS_ERREF_H */




