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

/*****************************************************************************/
struct xrdp_fastpath *
xrdp_fastpath_create(struct xrdp_sec *owner, struct trans *trans)
{
    struct xrdp_fastpath *self;

    DEBUG(("  in xrdp_fastpath_create"));
    self = (struct xrdp_fastpath *)g_malloc(sizeof(struct xrdp_fastpath), 1);
    self->sec_layer = owner;
    self->trans = trans;
    self->session = owner->rdp_layer->session;
    DEBUG(("  out xrdp_fastpath_create"));
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
/* returns error */
int
xrdp_fastpath_reset(struct xrdp_fastpath *self)
{
    return 0;
}

/*****************************************************************************/
int
xrdp_fastpath_recv(struct xrdp_fastpath *self, struct stream *s)
{
    int fp_hdr;
    int len = 0; /* unused */
    int byte;
    char *holdp;

    DEBUG(("   in xrdp_fastpath_recv"));
    holdp = s->p;
    if (!s_check_rem(s, 2))
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

        if (!s_check_rem(s, 1))
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
    DEBUG(("  out xrdp_fastpath_recv"));
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
    xrdp_fastpath_session_callback(self, 0x5556, 0, 0, 0, 0);
    return 0;
}

/*****************************************************************************/
/* FASTPATH_INPUT_EVENT_SCANCODE */
static int
xrdp_fastpath_process_EVENT_SCANCODE(struct xrdp_fastpath *self,
                                     int eventFlags, struct stream *s)
{
    int flags;
    int code;
    flags = 0;

    if (!s_check_rem(s, 1))
    {
        return 1;
    }
    in_uint8(s, code); /* keyCode (1 byte) */

    if ((eventFlags & FASTPATH_INPUT_KBDFLAGS_RELEASE))
    {
        flags |= KBD_FLAG_UP;
    }
    else
    {
        flags |= KBD_FLAG_DOWN;
    }

    if ((eventFlags & FASTPATH_INPUT_KBDFLAGS_EXTENDED))
        flags |= KBD_FLAG_EXT;

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

    if (!s_check_rem(s, 2 + 2 + 2))
    {
        return 1;
    }
    in_uint16_le(s, pointerFlags); /* pointerFlags (2 bytes) */
    in_uint16_le(s, xPos); /* xPos (2 bytes) */
    in_uint16_le(s, yPos); /* yPos (2 bytes) */

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

    if (!s_check_rem(s, 2 + 2 + 2))
    {
        return 1;
    }
    in_uint16_le(s, pointerFlags); /* pointerFlags (2 bytes) */
    in_uint16_le(s, xPos); /* xPos (2 bytes) */
    in_uint16_le(s, yPos); /* yPos (2 bytes) */

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

    flags = 0;
    if (!s_check_rem(s, 2))
    {
        return 1;
    }
    in_uint16_le(s, code); /* unicode (2 byte) */
    if (eventFlags & FASTPATH_INPUT_KBDFLAGS_RELEASE)
    {
        flags |= KBD_FLAG_UP;
    }
    else
    {
        flags |= KBD_FLAG_DOWN;
    }
    if (eventFlags & FASTPATH_INPUT_KBDFLAGS_EXTENDED)
    {
        flags |= KBD_FLAG_EXT;
    }
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
        if (!s_check_rem(s, 1))
        {
            return 1;
        }
        in_uint8(s, eventHeader);

        eventFlags = (eventHeader & 0x1F);
        eventCode = (eventHeader >> 5);

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
                g_writeln("xrdp_fastpath_process_input_event: unknown "
                          "eventCode %d", eventCode);
                break;
        }
    }
    return 0;
}
