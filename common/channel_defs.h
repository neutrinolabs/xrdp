/**
 * Copyright (C) 2024 Matt Burt, all xrdp contributors
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

/**
 * @file    common/channel_defs.h
 * @brief   Private xrdp channel definitions
 */

#if !defined(CHANNEL_DEFS_H)
#define CHANNEL_DEFS_H

#include <stdint.h>

/**
 * Channel IDs assigned to xrdp private channels. These are chosen to avoid conflicts with
 * channel IDs returned by other servers.
 */
enum
{
    CHAN_ID_XRDP_BASE = (('x' << 24) | ('r' << 16) | ('d' << 8) | 'p'),
    CHAN_ID_XRDP_SESSION_INFO = CHAN_ID_XRDP_BASE,
    // Add further IDs here
    CHAN_ID_XRDP_MAX
};

// Max length of a DVC name. This is taken from the specification of
// WTSVirtualChannelOpenEx() which limits the length of a virtual channel
// to Windows MAX_PATH
#define MAX_DVC_NAME_LEN 260

/**
 * Information to connect to a channel using xrdpapi
 */
#define XRDPAPI_CONNECT_PDU_LEN 384 // Connect PDU is always this length
#define XRDPAPI_CONNECT_PDU_VERSION 1 // Current connnect PDU version

struct xrdp_chan_connect
{
    uint32_t version;
    uint32_t private_chan; // 0 for a standard channel
    uint32_t flags;
    char name[MAX_DVC_NAME_LEN + 1]; // null-terminated
};

// Events received on the CHAN_ID_XRDP_SESSION_INFO channel

// Update on all the session state data held by chansrv
//
// If more information is needed to be exported by xrdpapi, it can be
// added to this event
struct xrdp_chan_session_state
{
    uint32_t is_connected; // Is the session connected?
};

#endif /* CHANNEL_DEFS_H */
