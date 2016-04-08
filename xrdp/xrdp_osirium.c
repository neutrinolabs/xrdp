/**
 * Patches to xrdp to read session details from a premable in the tcp stream
 *
 * Copyright (C) Osirium Ltd 2004-2012
 *
 */

#include "xrdp.h"

/*****************************************************************************/
static APP_CC
int lisspace(int c)
{
    /* space(SPC)
       horizontal tab (TAB)
       newline (LF)
       vertical tab (VT)
       feed (FF)
       carriage return (CR) */
    if (c == 0x20 || c == 0x09 || c == 0x0A || c == 0x0B || c == 0x0C || c == 0x0D)
    {
        return 1;
    }
    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
read_preamble_packet(struct xrdp_process* self)
{
    // Using the session stream
    int ns_len = 0;
    int idx;
    int len;
    // cache a pointer to client_info
    struct xrdp_client_info *client_info = self->session->client_info; 

    struct stream* s;
    make_stream(s);
    init_stream(s, 10000);

    DEBUG(("in read_preamble_packet"));

    // 5 bytes should include the colon  
    if (trans_force_read_s(self->server_trans, s, 5) == 0)
    {
        idx = g_pos(s->data, ":") + 1; // skip colon
        ns_len = g_atoi(s->data);
        DEBUG(("Preamble length %i", ns_len));
        len = ns_len - 5 + idx + 1; // trailing comma as well 
        DEBUG(("Preamble to read %i", len));
        if (trans_force_read_s(self->server_trans, s, len) == 0)
        {
            DEBUG(("Preamble body %s", s->data));
            // this will be used as a processing buffer when data required
            // so do not assume will exist in this state later on.
            client_info->osirium_preamble_buffer = (char*)g_malloc(ns_len+1, 1);
            in_uint8s(s, idx);    // skip netstring header 
            in_uint8a(s, 
                    client_info->osirium_preamble_buffer, 
                    ns_len);
            DEBUG(("Preamble %s", client_info->osirium_preamble_buffer));
            free_stream(s);
            return 0;
        }
    }
    free_stream(s);
    DEBUG(("out read_preamble_packet"));
    return 0;
}

/*****************************************************************************/
int APP_CC
process_preamble_packet(struct xrdp_wm *self)
{
    char* line_end;
    char* t;
    char* q;
    char* r;
    DEBUG(("Preamble %s", self->session->client_info->osirium_preamble_buffer))
    line_end = g_strchr(self->session->client_info->osirium_preamble_buffer, '\n');
    while (line_end != 0)
    {
        q = line_end+1;
        // locate start of name
        while ( lisspace(*q) )
        {
            q++;
        }
        DEBUG(("Preamble still needing processing %s", q));
        // locate separator
        t = r = g_strchr(q,'=');
        if ( r == 0 ) break;  // handle broken preamble by assuming at end of pre
        *r = 0; // ensure name terminated

        // strip possible trailing spaces from name
        while ( lisspace(*--t) )
        {
            *t = 0; // nulls to terminate name
        };
        // locate start of value
        while ( lisspace(*++r) )
        {
          // pre increment
        }

        line_end = g_strchr(r, '\n'); //locate end of value
        if (line_end)   // may be last value in preamble and have no LF at end.
        {
            *line_end = 0; // null terminate value
        }
        DEBUG(("Name '%s' Value '%s'", q, r));
        list_add_item(self->mm->login_names, (long)g_strdup(q));
        list_add_item(self->mm->login_values, (long)g_strdup(r));
    }
    g_free(self->session->client_info->osirium_preamble_buffer);
    self->session->client_info->osirium_preamble_buffer = 0;
    xrdp_wm_set_login_mode(self, 2);
    return 0;
}

