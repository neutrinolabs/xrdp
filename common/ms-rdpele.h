/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * MS-RDPELE : Definitions from [MS-RDPELE]
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
 * References to MS-RDPELE are currently correct for v20180912 of that
 * document
 */

#if !defined(MS_RDPELE_H)
#define MS_RDPELE_H

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

#endif /* MS_RDPELE_H */
