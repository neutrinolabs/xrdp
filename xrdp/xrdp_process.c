/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2014
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
 * main rdp process
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "xrdp.h"

static int g_session_id = 0;

/*****************************************************************************/
/* always called from xrdp_listen thread */
struct xrdp_process *
xrdp_process_create(struct xrdp_listen *owner, tbus done_event)
{
    struct xrdp_process *self;
    char event_name[256];
    int pid;

    self = (struct xrdp_process *)g_malloc(sizeof(struct xrdp_process), 1);
    self->lis_layer = owner;
    self->done_event = done_event;
    g_session_id++;
    self->session_id = g_session_id;
    pid = g_getpid();
    g_snprintf(event_name, 255, "xrdp_%8.8x_process_self_term_event_%8.8x",
               pid, self->session_id);
    self->self_term_event = g_create_wait_obj(event_name);
    return self;
}

/*****************************************************************************/
void
xrdp_process_delete(struct xrdp_process *self)
{
    if (self == 0)
    {
        return;
    }

    g_delete_wait_obj(self->self_term_event);
    libxrdp_exit(self->session);
    xrdp_wm_delete(self->wm);
    trans_delete(self->server_trans);
    g_free(self);
}

/*****************************************************************************/
static int
xrdp_process_loop(struct xrdp_process *self, struct stream *s)
{
    int rv;

    rv = 0;

    if (self->session != 0)
    {
        rv = libxrdp_process_data(self->session, s);

        if ((self->wm == 0) && (self->session->up_and_running) && (rv == 0))
        {
            DEBUG(("calling xrdp_wm_init and creating wm"));
            self->wm = xrdp_wm_create(self, self->session->client_info);
            /* at this point the wm(window manager) is create and wm::login_mode is
               zero and login_mode_event is set so xrdp_wm_init should be called by
               xrdp_wm_check_wait_objs */
        }
    }

    return rv;
}

/*****************************************************************************/
/* returns boolean */
/* this is so libxrdp.so can known when to quit looping */
static int
xrdp_is_term(void)
{
    return g_is_term();
}

/*****************************************************************************/
static int
xrdp_process_mod_end(struct xrdp_process *self)
{
    if (self->wm != 0)
    {
        if (self->wm->mm != 0)
        {
            if (self->wm->mm->mod != 0)
            {
                if (self->wm->mm->mod->mod_end != 0)
                {
                    return self->wm->mm->mod->mod_end(self->wm->mm->mod);
                }
            }
        }
    }

    return 0;
}

/*****************************************************************************/
static int
xrdp_process_data_in(struct trans *self)
{
    struct xrdp_process *pro;
    struct stream *s;
    int len;

    DEBUG(("xrdp_process_data_in"));
    pro = (struct xrdp_process *)(self->callback_data);

    s = pro->server_trans->in_s;
    switch (pro->server_trans->extra_flags)
    {
        case 0:
            /* early in connection sequence, we're in this mode */
            if (xrdp_process_loop(pro, 0) != 0)
            {
                g_writeln("xrdp_process_data_in: "
                          "xrdp_process_loop failed");
                return 1;
            }
            if (pro->session->up_and_running)
            {
                pro->server_trans->header_size = 2;
                pro->server_trans->extra_flags = 1;
                init_stream(s, 0);
            }
            break;

        case 1:
            /* we got 2 bytes */
            if (s->p[0] == 3)
            {
                pro->server_trans->header_size = 4;
                pro->server_trans->extra_flags = 2;
            }
            else
            {
                if (s->p[1] & 0x80)
                {
                    pro->server_trans->header_size = 3;
                    pro->server_trans->extra_flags = 2;
                }
                else
                {
                    len = (tui8)(s->p[1]);
                    pro->server_trans->header_size = len;
                    pro->server_trans->extra_flags = 3;
                }
            }

            len = (int) (s->end - s->data);
            if (pro->server_trans->header_size > len)
            {
                /* not enough data read yet */
                break;
            }
            /* FALLTHROUGH */

        case 2:
            /* we have enough now to get the PDU bytes */
            len = libxrdp_get_pdu_bytes(s->p);
            if (len == -1)
            {
                g_writeln("xrdp_process_data_in: "
                          "xrdp_process_get_packet_bytes failed");
                return 1;
            }
            pro->server_trans->header_size = len;
            pro->server_trans->extra_flags = 3;

            len = (int) (s->end - s->data);
            if (pro->server_trans->header_size > len)
            {
                /* not enough data read yet */
                break;
            }
            /* FALLTHROUGH */

        case 3:
            /* the whole PDU is read in now process */
            s->p = s->data;
            if (xrdp_process_loop(pro, s) != 0)
            {
                g_writeln("xrdp_process_data_in: "
                          "xrdp_process_loop failed");
                return 1;
            }
            init_stream(s, 0);
            pro->server_trans->header_size = 2;
            pro->server_trans->extra_flags = 1;
            break;
    }
    return 0;
}

/*****************************************************************************/
int
xrdp_process_main_loop(struct xrdp_process *self)
{
    int robjs_count;
    int wobjs_count;
    int cont;
    int timeout = 0;
    tbus robjs[32];
    tbus wobjs[32];
    tbus term_obj;

    DEBUG(("xrdp_process_main_loop"));
    self->status = 1;
    self->server_trans->extra_flags = 0;
    self->server_trans->header_size = 0;
    self->server_trans->no_stream_init_on_data_in = 1;
    self->server_trans->trans_data_in = xrdp_process_data_in;
    self->server_trans->callback_data = self;
    init_stream(self->server_trans->in_s, 8192 * 4);
    self->session = libxrdp_init((tbus)self, self->server_trans);
    self->server_trans->si = &(self->session->si);
    self->server_trans->my_source = XRDP_SOURCE_CLIENT;
    /* this callback function is in xrdp_wm.c */
    self->session->callback = callback;
    /* this function is just above */
    self->session->is_term = xrdp_is_term;

    if (libxrdp_process_incoming(self->session) == 0)
    {
        init_stream(self->server_trans->in_s, 32 * 1024);

        term_obj = g_get_term_event();
        cont = 1;

        while (cont)
        {
            /* build the wait obj list */
            timeout = -1;
            robjs_count = 0;
            wobjs_count = 0;
            robjs[robjs_count++] = term_obj;
            robjs[robjs_count++] = self->self_term_event;
            xrdp_wm_get_wait_objs(self->wm, robjs, &robjs_count,
                                  wobjs, &wobjs_count, &timeout);
            trans_get_wait_objs_rw(self->server_trans, robjs, &robjs_count,
                                   wobjs, &wobjs_count, &timeout);
            /* wait */
            if (g_obj_wait(robjs, robjs_count, wobjs, wobjs_count, timeout) != 0)
            {
                /* error, should not get here */
                g_sleep(100);
            }

            if (g_is_wait_obj_set(term_obj)) /* term */
            {
                break;
            }

            if (g_is_wait_obj_set(self->self_term_event))
            {
                break;
            }

            if (xrdp_wm_check_wait_objs(self->wm) != 0)
            {
                break;
            }

            if (trans_check_wait_objs(self->server_trans) != 0)
            {
                break;
            }
        }
        /* send disconnect message if possible */
        libxrdp_disconnect(self->session);
    }
    else
    {
        g_writeln("xrdp_process_main_loop: libxrdp_process_incoming failed");
        /* this will try to send a disconnect,
           maybe should check that connection got far enough */
        libxrdp_disconnect(self->session);
    }
    /* Run end in module */
    xrdp_process_mod_end(self);
    libxrdp_exit(self->session);
    self->session = 0;
    self->status = -1;
    g_set_wait_obj(self->done_event);
    return 0;
}
