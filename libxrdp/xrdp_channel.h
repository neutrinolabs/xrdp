/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * MS-RDPEDYC : Definitions related to documentation in [MS-RDPEDYC]
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
 * References to MS-RDPEDYC are currently correct for v20210406 of that
 * document
 */

#if !defined(XRDP_CHANNEL_H)
#define XRDP_CHANNEL_H

/*
   These are not directly defined in MS-RDPEDYC, but
   they are derived statuses needed to implement it.
*/
#define XRDP_DRDYNVC_STATUS_CLOSED          0
#define XRDP_DRDYNVC_STATUS_OPEN_SENT       1
#define XRDP_DRDYNVC_STATUS_OPEN            2
#define XRDP_DRDYNVC_STATUS_CLOSE_SENT      3

#define XRDP_DRDYNVC_STATUS_TO_STR(status) \
    ((status) == XRDP_DRDYNVC_STATUS_CLOSED ? "CLOSED" : \
     (status) == XRDP_DRDYNVC_STATUS_OPEN_SENT ? "OPEN_SENT" : \
     (status) == XRDP_DRDYNVC_STATUS_OPEN ? "OPEN" : \
     (status) == XRDP_DRDYNVC_STATUS_CLOSE_SENT ? "CLOSE_SENT" : \
     "unknown" \
    )

#endif /* XRDP_CHANNEL_H */
