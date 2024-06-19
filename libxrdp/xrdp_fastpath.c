/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2012-2014
 * Copyright (C) Idan Freiberg 2013-2014
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

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "libxrdp.h"
#include "ms-rdpbcgr.h"

/*****************************************************************************/
struct xrdp_fastpath *
xrdp_fastpath_create(struct xrdp_sec *owner, struct trans *trans)
{
    struct xrdp_fastpath *self;

    self = (struct xrdp_fastpath *)g_malloc(sizeof(struct xrdp_fastpath), 1);
    self->sec_layer = owner;
    self->trans = trans;
    self->session = owner->rdp_layer->session;

    return self;
}

/*****************************************************************************/
void
xrdp_fastpath_delete(struct xrdp_fastpath *self)
{
    if (self == 0)
    {
        return;
    }
    g_free(self);
}

/*****************************************************************************/
int
xrdp_fastpath_recv(struct xrdp_fastpath *self, struct stream *s)
{
    int fp_hdr;
    int len = 0;
    int byte;
    char *holdp;


    holdp = s->p;
    if (!s_check_rem_and_log(s, 2, "Parsing [MS-RDPBCGR] TS_FP_INPUT_PDU"))
    {
        return 1;
    }
    in_uint8(s, fp_hdr); /* fpInputHeader (1 byte) */
    in_uint8(s, byte); /* length 1 (1 byte) */

    self->numEvents = (fp_hdr & 0x3C) >> 2;
    self->secFlags = (fp_hdr & 0xC0) >> 6;

    if (byte & 0x80)
    {
        byte &= ~(0x80);
        len = (byte << 8);

        if (!s_check_rem_and_log(s, 1, "Parsing [MS-RDPBCGR] TS_FP_INPUT_PDU length2"))
        {
            return 1;
        }
        in_uint8(s, byte); /* length 2 (1 byte) */

        len += byte;
    }
    else
    {
        len = byte;
    }
    s->next_packet = holdp + len;
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received header [MS-RDPBCGR] TS_FP_INPUT_PDU "
              "fpInputHeader.action (ignored), fpInputHeader.numEvents %d, "
              "fpInputHeader.flags 0x%1.1x, length %d",
              self->numEvents, self->secFlags, len);
    return 0;
}

/*****************************************************************************/
/* no fragmentation */
int
xrdp_fastpath_init(struct xrdp_fastpath *self, struct stream *s)
{
    int bytes;

    bytes = self->session->client_info->max_fastpath_frag_bytes;
    if (bytes < 32 * 1024)
    {
        bytes = 32 * 1024;
    }
    init_stream(s, bytes);
    return 0;
}

/*****************************************************************************/
static int
xrdp_fastpath_session_callback(struct xrdp_fastpath *self, int msg,
                               long param1, long param2,
                               long param3, long param4)
{
    if (self->session->callback != 0)
    {
        /* msg_type can be
          RDP_INPUT_SYNCHRONIZE - 0
          RDP_INPUT_SCANCODE - 4
          RDP_INPUT_MOUSE - 0x8001
          RDP_INPUT_MOUSEX - 0x8002 */
        /* call to xrdp_wm.c : callback */
        self->session->callback(self->session->id, msg,
                                param1, param2, param3, param4);
    }
    else
    {
        LOG_DEVEL(LOG_LEVEL_WARNING, "Bug: session is NULL");
    }
    return 0;
}

/*****************************************************************************/
/* no fragmentation */
int
xrdp_fastpath_send(struct xrdp_fastpath *self, struct stream *s)
{
    if (trans_write_copy_s(self->trans, s) != 0)
    {
        return 1;
    }
    if (self->session->check_for_app_input)
    {
        xrdp_fastpath_session_callback(self, 0x5556, 0, 0, 0, 0);
    }
    return 0;
}

/*****************************************************************************/
/**
 * Converts the fastpath keyboard event flags to slowpath event flags
 * @param Faspath flags
 * @return slowpath flags
 *
 * See [MMS-RDPBCGR] 2.2.8.1.1.3.1.1.1 and 2.2.8.1.2.2.1
 */
static int
get_slowpath_keyboard_event_flags(int fp_flags)
{
    int flags = 0;

    if (fp_flags & FASTPATH_INPUT_KBDFLAGS_RELEASE)
    {
        // Assume the key was down prior to this event - the fastpath
        // message doesn't have a separate flag which maps to
        // KBDFLAGS_DOWN
        flags |= KBDFLAGS_DOWN | KBDFLAGS_RELEASE;
    }
    if (fp_flags & FASTPATH_INPUT_KBDFLAGS_EXTENDED)
    {
        flags |= KBDFLAGS_EXTENDED;
    }
    if (fp_flags & FASTPATH_INPUT_KBDFLAGS_EXTENDED1)
    {
        flags |= KBDFLAGS_EXTENDED1;
    }

    return flags;
}

/*****************************************************************************/
/* FASTPATH_INPUT_EVENT_SCANCODE */
static int
xrdp_fastpath_process_EVENT_SCANCODE(struct xrdp_fastpath *self,
                                     int eventFlags, struct stream *s)
{
    int flags;
    int code;

    if (!s_check_rem_and_log(s, 1, "Parsing [MS-RDPBCGR] TS_FP_KEYBOARD_EVENT"))
    {
        return 1;
    }
    in_uint8(s, code); /* keyCode (1 byte) */
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_FP_KEYBOARD_EVENT "
              "eventHeader.eventFlags 0x%2.2x, eventHeader.eventCode (ignored), "
              "keyCode %d", eventFlags, code);

    flags = get_slowpath_keyboard_event_flags(eventFlags);

    xrdp_fastpath_session_callback(self, RDP_INPUT_SCANCODE,
                                   code, 0, flags, 0);

    return 0;
}

/*****************************************************************************/
/* FASTPATH_INPUT_EVENT_MOUSE */
static int
xrdp_fastpath_process_EVENT_MOUSE(struct xrdp_fastpath *self,
                                  int eventFlags, struct stream *s)
{
    int pointerFlags;
    int xPos;
    int yPos;

    /* eventFlags MUST be zeroed out */
    if (eventFlags != 0)
    {
        return 1;
    }

    if (!s_check_rem_and_log(s, 2 + 2 + 2, "Parsing [MS-RDPBCGR] TS_FP_POINTER_EVENT"))
    {
        return 1;
    }
    in_uint16_le(s, pointerFlags); /* pointerFlags (2 bytes) */
    in_uint16_le(s, xPos); /* xPos (2 bytes) */
    in_uint16_le(s, yPos); /* yPos (2 bytes) */
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_FP_POINTER_EVENT "
              "eventHeader.eventFlags 0x00, eventHeader.eventCode (ignored), "
              "pointerFlags 0x%4.4x, xPos %d, yPos %d", pointerFlags, xPos, yPos);

    xrdp_fastpath_session_callback(self, RDP_INPUT_MOUSE,
                                   xPos, yPos, pointerFlags, 0);

    return 0;
}

/*****************************************************************************/
/* FASTPATH_INPUT_EVENT_MOUSEX */
static int
xrdp_fastpath_process_EVENT_MOUSEX(struct xrdp_fastpath *self,
                                   int eventFlags, struct stream *s)
{
    int pointerFlags;
    int xPos;
    int yPos;

    /* eventFlags MUST be zeroed out */
    if (eventFlags != 0)
    {
        return 1;
    }

    if (!s_check_rem_and_log(s, 2 + 2 + 2,
                             "Parsing [MS-RDPBCGR] TS_FP_POINTERX_EVENT"))
    {
        return 1;
    }
    in_uint16_le(s, pointerFlags); /* pointerFlags (2 bytes) */
    in_uint16_le(s, xPos); /* xPos (2 bytes) */
    in_uint16_le(s, yPos); /* yPos (2 bytes) */
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_FP_POINTERX_EVENT "
              "eventHeader.eventFlags 0x%2.2x, eventHeader.eventCode (ignored), "
              "pointerFlags 0x%4.4x, xPos %d, yPos %d",
              eventFlags, pointerFlags, xPos, yPos);

    xrdp_fastpath_session_callback(self, RDP_INPUT_MOUSEX,
                                   xPos, yPos, pointerFlags, 0);

    return 0;
}

/*****************************************************************************/
/* FASTPATH_INPUT_EVENT_SYNC */
static int
xrdp_fastpath_process_EVENT_SYNC(struct xrdp_fastpath *self,
                                 int eventFlags, struct stream *s)
{
    /*
     * The eventCode bitfield (3 bits in size) MUST be set to
     * FASTPATH_INPUT_EVENT_SYNC (3).
     * The eventFlags bitfield (5 bits in size) contains flags
     * indicating the "on"
     * status of the keyboard toggle keys.
     */

    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_FP_SYNC_EVENT"
              "eventHeader.eventFlags 0x%2.2x, eventHeader.eventCode (ignored), ",
              eventFlags);

    xrdp_fastpath_session_callback(self, RDP_INPUT_SYNCHRONIZE,
                                   eventFlags, 0, 0, 0);

    return 0;
}

/*****************************************************************************/
/* FASTPATH_INPUT_EVENT_UNICODE */
static int
xrdp_fastpath_process_EVENT_UNICODE(struct xrdp_fastpath *self,
                                    int eventFlags, struct stream *s)
{
    int flags;
    int code;

    if (!s_check_rem_and_log(s, 2, "Parsing [MS-RDPBCGR] TS_FP_UNICODE_KEYBOARD_EVENT"))
    {
        return 1;
    }
    in_uint16_le(s, code); /* unicode (2 byte) */
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_FP_UNICODE_KEYBOARD_EVENT"
              "eventHeader.eventFlags 0x%2.2x, eventHeader.eventCode (ignored), "
              "unicodeCode %d",
              eventFlags, code);

    flags = get_slowpath_keyboard_event_flags(eventFlags);

    xrdp_fastpath_session_callback(self, RDP_INPUT_UNICODE,
                                   code, 0, flags, 0);
    return 0;
}

/*****************************************************************************/
/* FASTPATH_INPUT_EVENT */
int
xrdp_fastpath_process_input_event(struct xrdp_fastpath *self,
                                  struct stream *s)
{
    int i;
    int eventHeader;
    int eventCode;
    int eventFlags;

    /* process fastpath input events */
    for (i = 0; i < self->numEvents; i++)
    {
        if (!s_check_rem_and_log(s, 1, "Parsing [MS-RDPBCGR] TS_FP_INPUT_EVENT eventHeader"))
        {
            return 1;
        }
        in_uint8(s, eventHeader);

        eventFlags = (eventHeader & 0x1F);
        eventCode = (eventHeader >> 5);
        LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_FP_INPUT_EVENT"
                  "eventHeader.eventFlags 0x%2.2x, eventHeader.eventCode 0x%1.1x",
                  eventFlags, eventCode);

        switch (eventCode)
        {
            case FASTPATH_INPUT_EVENT_SCANCODE:
                if (xrdp_fastpath_process_EVENT_SCANCODE(self,
                        eventFlags,
                        s) != 0)
                {
                    return 1;
                }
                break;
            case FASTPATH_INPUT_EVENT_MOUSE:
                if (xrdp_fastpath_process_EVENT_MOUSE(self,
                                                      eventFlags,
                                                      s) != 0)
                {
                    return 1;
                }
                break;
            case FASTPATH_INPUT_EVENT_MOUSEX:
                if (xrdp_fastpath_process_EVENT_MOUSEX(self,
                                                       eventFlags,
                                                       s) != 0)
                {
                    return 1;
                }
                break;
            case FASTPATH_INPUT_EVENT_SYNC:
                if (xrdp_fastpath_process_EVENT_SYNC(self,
                                                     eventFlags,
                                                     s) != 0)
                {
                    return 1;
                }
                break;
            case FASTPATH_INPUT_EVENT_UNICODE:
                if (xrdp_fastpath_process_EVENT_UNICODE(self,
                                                        eventFlags,
                                                        s) != 0)
                {
                    return 1;
                }
                break;
            default:
                LOG(LOG_LEVEL_ERROR, "xrdp_fastpath_process_input_event: "
                    "unknown eventCode %d", eventCode);
                break;
        }
    }
    return 0;
}
