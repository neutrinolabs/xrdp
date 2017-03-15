/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2013 jay.sorg@gmail.com
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
 */

/*
 * smartcard redirection support, PCSC daemon standin
 * this will act like pcsc daemon
 * pcsc lib and daemon write struct on unix domain socket for communication
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#define JAY_TODO_CONTEXT    0
#define JAY_TODO_WIDE       1

#define PCSC_STANDIN 1

#include "os_calls.h"
#include "smartcard.h"
#include "log.h"
#include "irp.h"
#include "devredir.h"
#include "trans.h"
#include "chansrv.h"
#include "list.h"

#if PCSC_STANDIN

#define LLOG_LEVEL 1
#define LLOGLN(_level, _args) \
  do \
  { \
    if (_level < LLOG_LEVEL) \
    { \
      g_write("chansrv:smartcard_pcsc [%10.10u]: ", g_time3()); \
      g_writeln _args ; \
    } \
  } \
  while (0)

extern int g_display_num; /* in chansrv.c */

static int g_autoinc = 0; /* general purpose autoinc */

struct pcsc_card /* item for list of open cards in one context */
{
    tui32 app_card;  /* application card, always 4 bytes */
    int card_bytes;  /* client card bytes */
    char card[16];   /* client card */
};

struct pcsc_context
{
    tui32 app_context;  /* application context, always 4 byte */
    int context_bytes;  /* client context bytes */
    char context[16];   /* client context */
    struct list *cards; /* these need to be released on close */
};

/*****************************************************************************/
struct pcsc_uds_client
{
    int uds_client_id;     /* unique id represents each app */
    struct trans *con;     /* the connection to the app */
    struct list *contexts; /* list of struct pcsc_context */
    struct pcsc_context *connect_context;
};

static struct list *g_uds_clients = 0; /* struct pcsc_uds_client */

static struct trans *g_lis = 0;
static char g_pcsclite_ipc_dir[256] = "";
static char g_pcsclite_ipc_file[256] = "";

/*****************************************************************************/
/* got a new unix domain socket connection */
static struct pcsc_uds_client *
create_uds_client(struct trans *con)
{
    struct pcsc_uds_client *uds_client;

    LLOGLN(10, ("create_uds_client:"));
    if (con == 0)
    {
        return 0;
    }
    uds_client = g_new0(struct pcsc_uds_client, 1);
    if (uds_client == 0)
    {
        return 0;
    }
    g_autoinc++;
    uds_client->uds_client_id = g_autoinc;
    uds_client->con = con;
    con->callback_data = uds_client;
    return uds_client;
}

/*****************************************************************************/
static struct pcsc_uds_client *
get_uds_client_by_id(int uds_client_id)
{
    struct pcsc_uds_client *uds_client;
    int index;

    LLOGLN(10, ("get_uds_client_by_id:"));
    if (uds_client_id == 0)
    {
        LLOGLN(10, ("get_uds_client_by_id: uds_client_id zero"));
        return 0;
    }
    if (g_uds_clients == 0)
    {
        LLOGLN(10, ("get_uds_client_by_id: g_uds_clients is nil"));
        return 0;
    }
    LLOGLN(10, ("  count %d", g_uds_clients->count));
    for (index = 0; index < g_uds_clients->count; index++)
    {
        uds_client = (struct pcsc_uds_client *)
                     list_get_item(g_uds_clients, index);
        if (uds_client->uds_client_id == uds_client_id)
        {
            return uds_client;
        }
    }
    LLOGLN(10, ("get_uds_client_by_id: can't find uds_client_id %d",
           uds_client_id));
    return 0;
}

/*****************************************************************************/
struct pcsc_context *
get_pcsc_context_by_app_context(struct pcsc_uds_client *uds_client,
                                tui32 app_context)
{
    struct pcsc_context *rv;
    int index;

    if (uds_client == 0)
    {
        return 0;
    }
    if (uds_client->contexts == 0)
    {
        return 0;
    }
    for (index = 0; index < uds_client->contexts->count; index++)
    {
        rv = (struct pcsc_context *)
             list_get_item(uds_client->contexts, index);
        if (rv->app_context == app_context)
        {
            return rv;
        }
    }
    return 0;
}

/*****************************************************************************/
struct pcsc_card *
get_pcsc_card_by_app_card(struct pcsc_uds_client *uds_client,
                          tui32 app_card, struct pcsc_context **acontext)
{
    struct pcsc_card *lcard;
    struct pcsc_context *lcontext;
    int index;
    int index1;

    LLOGLN(10, ("get_pcsc_card_by_app_card:"));
    LLOGLN(10, ("  app_card %d", app_card));
    if (uds_client == 0)
    {
        LLOGLN(0, ("get_pcsc_card_by_app_card: error"));
        return 0;
    }
    if (uds_client->contexts == 0)
    {
        LLOGLN(0, ("get_pcsc_card_by_app_card: error"));
        return 0;
    }
    for (index = 0; index < uds_client->contexts->count; index++)
    {
        lcontext = (struct pcsc_context *)
                   list_get_item(uds_client->contexts, index);
        if (lcontext != 0)
        {
            if (lcontext->cards != 0)
            {
                for (index1 = 0; index1 < lcontext->cards->count; index1++)
                {
                    lcard = (struct pcsc_card *)
                            list_get_item(lcontext->cards, index1);
                    if (lcard != 0)
                    {
                        if (lcard->app_card == app_card)
                        {
                            if (acontext != 0)
                            {
                                *acontext = lcontext;
                            }
                            return lcard;
                        }
                    }
                }
            }
        }
    }
    LLOGLN(0, ("get_pcsc_card_by_app_card: error"));
    return 0;
}

/*****************************************************************************/
static int
free_uds_client(struct pcsc_uds_client *uds_client)
{
    int i;
    int j;
    struct pcsc_context *context;
    struct pcsc_card *card;

    LLOGLN(10, ("free_uds_client:"));
    if (uds_client == 0)
    {
        return 0;
    }
    if (uds_client->contexts != 0)
    {
        for (i = 0; i < uds_client->contexts->count; i++)
        {
            context = (struct pcsc_context *)
                      list_get_item(uds_client->contexts, i);
            if (context != 0)
            {
                if (context->cards != 0)
                {
                    for (j = 0; j < context->cards->count; j++)
                    {
                        card = (struct pcsc_card *)
                               list_get_item(context->cards, j);
                        if (card != 0)
                        {
                            /* TODO: send free card to client */
                            g_free(card);
                        }
                    }
                    list_delete(context->cards);
                }
                LLOGLN(10, ("  left over context %p", context->context));
                scard_send_cancel(0, context->context, context->context_bytes);
                scard_send_release_context(0, context->context,
                                           context->context_bytes);
                g_free(context);
            }
        }
        list_delete(uds_client->contexts);
    }
    trans_delete(uds_client->con);
    g_free(uds_client);
    return 0;
}

/*****************************************************************************/
static struct pcsc_context *
uds_client_add_context(struct pcsc_uds_client *uds_client,
                       char *context, int context_bytes)
{
    struct pcsc_context *pcscContext;

    LLOGLN(10, ("uds_client_add_context:"));
    pcscContext = (struct pcsc_context *)
                  g_malloc(sizeof(struct pcsc_context), 1);
    if (pcscContext == 0)
    {
        LLOGLN(0, ("uds_client_add_context: error"));
        return 0;
    }
    g_autoinc++;
    pcscContext->app_context = g_autoinc;
    pcscContext->context_bytes = context_bytes;
    g_memcpy(pcscContext->context, context, context_bytes);
    if (uds_client->contexts == 0)
    {
        uds_client->contexts = list_create();
        if (uds_client->contexts == 0)
        {
            LLOGLN(0, ("uds_client_add_context: error"));
            return 0;
        }
    }
    list_add_item(uds_client->contexts, (tintptr) pcscContext);
    return pcscContext;
}

/*****************************************************************************/
static int
uds_client_remove_context(struct pcsc_uds_client *uds_client,
                          struct pcsc_context *acontext)
{
    int index;

    if (uds_client->contexts == 0)
    {
        LLOGLN(0, ("uds_client_remove_context: error"));
        return 1;
    }
    index = list_index_of(uds_client->contexts, (tintptr) acontext);
    if (index < 0)
    {
        LLOGLN(0, ("uds_client_remove_context: error"));
        return 1;
    }
    list_remove_item(uds_client->contexts, index);
    g_free(acontext); // TODO free cards
    return 0;
}

/*****************************************************************************/
static struct pcsc_card *
context_add_card(struct pcsc_uds_client *uds_client,
                 struct pcsc_context *acontext,
                 char *card, int card_bytes)
{
    struct pcsc_card *pcscCard;

    LLOGLN(10, ("context_add_card: card_bytes %d", card_bytes));
    pcscCard = (struct pcsc_card *)
               g_malloc(sizeof(struct pcsc_card), 1);
    if (pcscCard == 0)
    {
        LLOGLN(0, ("context_add_card: error"));
        return 0;
    }
    g_autoinc++;
    pcscCard->app_card = g_autoinc;
    pcscCard->card_bytes = card_bytes;
    g_memcpy(pcscCard->card, card, card_bytes);
    if (acontext->cards == 0)
    {
        acontext->cards = list_create();
        if (acontext->cards == 0)
        {
            LLOGLN(0, ("context_add_card: error"));
            return 0;
        }
    }
    list_add_item(acontext->cards, (tintptr) pcscCard);
    LLOGLN(10, ("  new app_card %d", pcscCard->app_card));
    return pcscCard;
}

/*****************************************************************************/
int
scard_pcsc_get_wait_objs(tbus *objs, int *count, int *timeout)
{
    struct pcsc_uds_client *uds_client;
    int index;

    LLOGLN(10, ("scard_pcsc_get_wait_objs:"));
    if (g_lis != 0)
    {
        trans_get_wait_objs(g_lis, objs, count);
    }
    if (g_uds_clients != 0)
    {
        for (index = 0; index < g_uds_clients->count; index++)
        {
            uds_client = (struct pcsc_uds_client *)
                         list_get_item(g_uds_clients, index);
            if (uds_client != 0)
            {
                trans_get_wait_objs(uds_client->con, objs, count);
            }
        }
    }
    return 0;
}

/*****************************************************************************/
int
scard_pcsc_check_wait_objs(void)
{
    struct pcsc_uds_client *uds_client;
    int index;

    LLOGLN(10, ("scard_pcsc_check_wait_objs:"));
    if (g_lis != 0)
    {
        if (trans_check_wait_objs(g_lis) != 0)
        {
            LLOGLN(0, ("scard_pcsc_check_wait_objs: g_lis trans_check_wait_objs error"));
        }
    }
    if (g_uds_clients != 0)
    {
        index = 0;
        while (index < g_uds_clients->count)
        {
            uds_client = (struct pcsc_uds_client *)
                         list_get_item(g_uds_clients, index);
            if (uds_client != 0)
            {
                if (trans_check_wait_objs(uds_client->con) != 0)
                {
                    free_uds_client(uds_client);
                    list_remove_item(g_uds_clients, index);
                    continue;
                }
            }
            index++;
        }
    }
    return 0;
}

/*****************************************************************************/
/* returns error */
int
scard_process_establish_context(struct trans *con, struct stream *in_s)
{
    int dwScope;
    struct pcsc_uds_client *uds_client;
    void *user_data;

    LLOGLN(10, ("scard_process_establish_context:"));
    uds_client = (struct pcsc_uds_client *) (con->callback_data);
    in_uint32_le(in_s, dwScope);
    LLOGLN(10, ("scard_process_establish_context: dwScope 0x%8.8x", dwScope));
    user_data = (void *) (tintptr) (uds_client->uds_client_id);
    scard_send_establish_context(user_data, dwScope);
    return 0;
}

/*****************************************************************************/
/* returns error */
int
scard_function_establish_context_return(void *user_data,
                                        struct stream *in_s,
                                        int len, int status)
{
    int bytes;
    int uds_client_id;
    int context_bytes;
    int app_context;
    char context[16];
    struct stream *out_s;
    struct pcsc_uds_client *uds_client;
    struct trans *con;
    struct pcsc_context *lcontext;

    LLOGLN(10, ("scard_function_establish_context_return:"));
    LLOGLN(10, ("  status 0x%8.8x", status));
    uds_client_id = (int) (tintptr) user_data;
    uds_client = (struct pcsc_uds_client *)
                 get_uds_client_by_id(uds_client_id);
    if (uds_client == 0)
    {
        LLOGLN(0, ("scard_function_establish_context_return: "
               "get_uds_client_by_id failed"));
        return 1;
    }
    con = uds_client->con;
    lcontext = 0;
    app_context = 0;
    g_memset(context, 0, 16);
    if (status == 0)
    {
        in_uint8s(in_s, 28);
        in_uint32_le(in_s, context_bytes);
        if (context_bytes > 16)
        {
            LLOGLN(0, ("scard_function_establish_context_return: opps "
                   "context_bytes %d", context_bytes));
            g_hexdump(in_s->p, context_bytes);
            return 1;
        }
        in_uint8a(in_s, context, context_bytes);
        lcontext = uds_client_add_context(uds_client, context, context_bytes);
        app_context = lcontext->app_context;
        LLOGLN(10, ("scard_function_establish_context_return: "
               "app_context %d", app_context));
    }
    out_s = trans_get_out_s(con, 8192);
    s_push_layer(out_s, iso_hdr, 8);
    out_uint32_le(out_s, app_context);
    out_uint32_le(out_s, status); /* SCARD_S_SUCCESS status */
    s_mark_end(out_s);
    bytes = (int) (out_s->end - out_s->data);
    s_pop_layer(out_s, iso_hdr);
    out_uint32_le(out_s, bytes - 8);
    out_uint32_le(out_s, 0x01); /* SCARD_ESTABLISH_CONTEXT 0x01 */
    return trans_force_write(con);
}

/*****************************************************************************/
/* returns error */
int
scard_process_release_context(struct trans *con, struct stream *in_s)
{
    int hContext;
    struct pcsc_uds_client *uds_client;
    struct pcsc_context *lcontext;
    void *user_data;

    LLOGLN(10, ("scard_process_release_context:"));
    uds_client = (struct pcsc_uds_client *) (con->callback_data);
    in_uint32_le(in_s, hContext);
    LLOGLN(10, ("scard_process_release_context: hContext 0x%8.8x", hContext));
    user_data = (void *) (tintptr) (uds_client->uds_client_id);
    lcontext = get_pcsc_context_by_app_context(uds_client, hContext);
    if (lcontext == 0)
    {
        LLOGLN(0, ("scard_process_release_context: "
               "get_pcsc_context_by_app_context failed"));
        return 1;
    }
    scard_send_release_context(user_data, lcontext->context,
                               lcontext->context_bytes);
    uds_client_remove_context(uds_client, lcontext);
    return 0;
}

/*****************************************************************************/
/* returns error */
int
scard_function_release_context_return(void *user_data,
                                      struct stream *in_s,
                                      int len, int status)
{
    int bytes;
    int uds_client_id;
    struct stream *out_s;
    struct pcsc_uds_client *uds_client;
    struct trans *con;

    LLOGLN(10, ("scard_function_release_context_return:"));
    LLOGLN(10, ("  status 0x%8.8x", status));
    uds_client_id = (int) (tintptr) user_data;
    uds_client = (struct pcsc_uds_client *)
                 get_uds_client_by_id(uds_client_id);
    if (uds_client == 0)
    {
        LLOGLN(0, ("scard_function_release_context_return: "
               "get_uds_client_by_id failed"));
        return 1;
    }
    con = uds_client->con;
    out_s = trans_get_out_s(con, 8192);
    s_push_layer(out_s, iso_hdr, 8);
    out_uint32_le(out_s, status); /* SCARD_S_SUCCESS status */
    s_mark_end(out_s);
    bytes = (int) (out_s->end - out_s->data);
    s_pop_layer(out_s, iso_hdr);
    out_uint32_le(out_s, bytes - 8);
    out_uint32_le(out_s, 0x02); /* SCARD_RELEASE_CONTEXT 0x02 */
    return trans_force_write(con);
}

/*****************************************************************************/
struct pcsc_list_readers
{
    int uds_client_id;
    int cchReaders;
};

/*****************************************************************************/
/* returns error */
int
scard_process_list_readers(struct trans *con, struct stream *in_s)
{
    int hContext;
    int bytes_groups;
    int cchReaders;
    char *groups;
    struct pcsc_uds_client *uds_client;
    struct pcsc_context *lcontext;
    struct pcsc_list_readers *pcscListReaders;

    LLOGLN(10, ("scard_process_list_readers:"));
    uds_client = (struct pcsc_uds_client *) (con->callback_data);
    in_uint32_le(in_s, hContext);
    in_uint32_le(in_s, bytes_groups);
    groups = (char *) g_malloc(bytes_groups + 1, 1);
    in_uint8a(in_s, groups, bytes_groups);
    in_uint32_le(in_s, cchReaders);
    LLOGLN(10, ("scard_process_list_readers: hContext 0x%8.8x cchReaders %d",
           hContext, cchReaders));
    lcontext = get_pcsc_context_by_app_context(uds_client, hContext);
    if (lcontext == 0)
    {
        LLOGLN(0, ("scard_process_list_readers: "
               "get_pcsc_context_by_app_context failed"));
        g_free(groups);
        return 1;
    }
    pcscListReaders = g_new0(struct pcsc_list_readers, 1);
    pcscListReaders->uds_client_id = uds_client->uds_client_id;
    pcscListReaders->cchReaders = cchReaders;
    scard_send_list_readers(pcscListReaders, lcontext->context,
                            lcontext->context_bytes, groups, cchReaders, 1);
    g_free(groups);
    return 0;
}

/*****************************************************************************/
int
scard_function_list_readers_return(void *user_data,
                                   struct stream *in_s,
                                   int len, int status)
{
    struct stream *out_s;
    int            chr;
    int            readers;
    int            rn_index;
    int            index;
    int            bytes;
    int            cchReaders;
    int            llen;
    int uds_client_id;
    twchar         reader_name[100];
    char           lreader_name[16][100];
    struct pcsc_uds_client *uds_client;
    struct trans *con;
    struct pcsc_list_readers *pcscListReaders;

    LLOGLN(10, ("scard_function_list_readers_return:"));
    LLOGLN(10, ("  status 0x%8.8x", status));
    pcscListReaders = (struct pcsc_list_readers *) user_data;
    if (pcscListReaders == 0)
    {
        LLOGLN(0, ("scard_function_list_readers_return: "
               "pcscListReaders is nil"));
        return 1;
    }
    uds_client_id = pcscListReaders->uds_client_id;
    cchReaders = pcscListReaders->cchReaders;
    g_free(pcscListReaders);
    uds_client = (struct pcsc_uds_client *)
                 get_uds_client_by_id(uds_client_id);
    if (uds_client == 0)
    {
        LLOGLN(0, ("scard_function_list_readers_return: "
               "get_uds_client_by_id failed, could not find id %d",
               uds_client_id));
        return 1;
    }
    con = uds_client->con;
    g_memset(reader_name, 0, sizeof(reader_name));
    g_memset(lreader_name, 0, sizeof(lreader_name));
    rn_index = 0;
    readers = 0;
    llen = 0;
    if (status == 0)
    {
        in_uint8s(in_s, 28);
        in_uint32_le(in_s, len);
        llen = len;
        if (cchReaders > 0)
        {
            while (len > 0)
            {
                in_uint16_le(in_s, chr);
                len -= 2;
                if (chr == 0)
                {
                    if (reader_name[0] != 0)
                    {
                        g_wcstombs(lreader_name[readers], reader_name, 99);
                        g_memset(reader_name, 0, sizeof(reader_name));
                        readers++;
                    }
                    reader_name[0] = 0;
                    rn_index = 0;
                }
                else
                {
                    reader_name[rn_index] = chr;
                    rn_index++;
                }
            }
        }
        if (rn_index > 0)
        {
            if (reader_name[0] != 0)
            {
                g_wcstombs(lreader_name[readers], reader_name, 99);
                g_memset(reader_name, 0, sizeof(reader_name));
                readers++;
            }
        }
    }

    out_s = trans_get_out_s(con, 8192);
    s_push_layer(out_s, iso_hdr, 8);
    out_uint32_le(out_s, llen);
    out_uint32_le(out_s, readers);
    for (index = 0; index < readers; index++)
    {
        out_uint8a(out_s, lreader_name[index], 100);
    }
    out_uint32_le(out_s, status); /* SCARD_S_SUCCESS status */
    s_mark_end(out_s);
    bytes = (int) (out_s->end - out_s->data);
    s_pop_layer(out_s, iso_hdr);
    out_uint32_le(out_s, bytes - 8);
    out_uint32_le(out_s, 0x03); /* SCARD_LIST_READERS 0x03 */
    return trans_force_write(con);
}

/*****************************************************************************/
/* returns error */
int
scard_process_connect(struct trans *con, struct stream *in_s)
{
    int hContext;
    READER_STATE rs;
    struct pcsc_uds_client *uds_client;
    void *user_data;
    struct pcsc_context *lcontext;

    LLOGLN(10, ("scard_process_connect:"));
    uds_client = (struct pcsc_uds_client *) (con->callback_data);
    g_memset(&rs, 0, sizeof(rs));
    in_uint32_le(in_s, hContext);
    in_uint8a(in_s, rs.reader_name, 100);
    in_uint32_le(in_s, rs.dwShareMode);
    in_uint32_le(in_s, rs.dwPreferredProtocols);
    LLOGLN(10, ("scard_process_connect: rs.reader_name %s dwShareMode 0x%8.8x "
           "dwPreferredProtocols 0x%8.8x", rs.reader_name, rs.dwShareMode,
           rs.dwPreferredProtocols));
    user_data = (void *) (tintptr) (uds_client->uds_client_id);
    lcontext = get_pcsc_context_by_app_context(uds_client, hContext);
    if (lcontext == 0)
    {
        LLOGLN(0, ("scard_process_connect: "
               "get_pcsc_context_by_app_context failed"));
        return 1;
    }
    uds_client->connect_context = lcontext;
    scard_send_connect(user_data, lcontext->context, lcontext->context_bytes,
                       1, &rs);
    return 0;
}

/*****************************************************************************/
int
scard_function_connect_return(void *user_data,
                              struct stream *in_s,
                              int len, int status)
{
    int dwActiveProtocol;
    int hCard;
    int bytes;
    int uds_client_id;
    struct stream *out_s;
    struct pcsc_uds_client *uds_client;
    struct trans *con;
    char *card;
    int card_bytes;
    struct pcsc_card *lcard;

    LLOGLN(10, ("scard_function_connect_return:"));
    LLOGLN(10, ("  status 0x%8.8x", status));
    uds_client_id = (int) (tintptr) user_data;
    uds_client = (struct pcsc_uds_client *)
                 get_uds_client_by_id(uds_client_id);
    if (uds_client == 0)
    {
        LLOGLN(0, ("scard_function_connect_return: "
               "get_uds_client_by_id failed"));
        return 1;
    }
    con = uds_client->con;
    dwActiveProtocol = 0;
    hCard = 0;
    if (status == 0)
    {
        in_uint8s(in_s, 36);
        in_uint32_le(in_s, dwActiveProtocol);
        if (len > 40)
        {
            in_uint32_le(in_s, card_bytes);
            in_uint8p(in_s, card, card_bytes);
            lcard = context_add_card(uds_client, uds_client->connect_context,
                                     card, card_bytes);
            hCard = lcard->app_card;
            LLOGLN(10, ("  hCard %d dwActiveProtocol %d", hCard,
                   dwActiveProtocol));
        }
        else
        {
            status = 0x8010000F; /* SCARD_E_PROTO_MISMATCH */
        }
    }
    out_s = trans_get_out_s(con, 8192);
    s_push_layer(out_s, iso_hdr, 8);
    out_uint32_le(out_s, hCard);
    out_uint32_le(out_s, dwActiveProtocol);
    out_uint32_le(out_s, status); /* SCARD_S_SUCCESS status */
    s_mark_end(out_s);
    bytes = (int) (out_s->end - out_s->data);
    s_pop_layer(out_s, iso_hdr);
    out_uint32_le(out_s, bytes - 8);
    out_uint32_le(out_s, 0x04); /* SCARD_CONNECT 0x04 */
    return trans_force_write(con);
}

/*****************************************************************************/
/* returns error */
int
scard_process_disconnect(struct trans *con, struct stream *in_s)
{
    int hCard;
    int dwDisposition;
    struct pcsc_uds_client *uds_client;
    void *user_data;
    struct pcsc_context *lcontext;
    struct pcsc_card *lcard;

    LLOGLN(10, ("scard_process_disconnect:"));
    uds_client = (struct pcsc_uds_client *) (con->callback_data);
    in_uint32_le(in_s, hCard);
    in_uint32_le(in_s, dwDisposition);
    user_data = (void *) (tintptr) (uds_client->uds_client_id);
    lcard = get_pcsc_card_by_app_card(uds_client, hCard, &lcontext);
    if ((lcontext == 0) || (lcard == 0))
    {
        LLOGLN(0, ("scard_process_disconnect: "
               "get_pcsc_card_by_app_card failed"));
        return 1;
    }
    scard_send_disconnect(user_data,
                          lcontext->context, lcontext->context_bytes,
                          lcard->card, lcard->card_bytes, dwDisposition);
    return 0;
}

/*****************************************************************************/
int
scard_function_disconnect_return(void *user_data,
                                 struct stream *in_s,
                                 int len, int status)
{
    int bytes;
    int uds_client_id;
    struct stream *out_s;
    struct pcsc_uds_client *uds_client;
    struct trans *con;

    LLOGLN(10, ("scard_function_disconnect_return:"));
    LLOGLN(10, ("  status 0x%8.8x", status));
    uds_client_id = (int) (tintptr) user_data;
    uds_client = (struct pcsc_uds_client *)
                 get_uds_client_by_id(uds_client_id);
    if (uds_client == 0)
    {
        LLOGLN(0, ("scard_function_disconnect_return: "
               "get_uds_client_by_id failed"));
        return 1;
    }
    con = uds_client->con;
    out_s = trans_get_out_s(con, 8192);
    s_push_layer(out_s, iso_hdr, 8);
    out_uint32_le(out_s, status); /* SCARD_S_SUCCESS status */
    s_mark_end(out_s);
    bytes = (int) (out_s->end - out_s->data);
    s_pop_layer(out_s, iso_hdr);
    out_uint32_le(out_s, bytes - 8);
    out_uint32_le(out_s, 0x06); /* SCARD_DISCONNECT 0x06 */
    return trans_force_write(con);
}

/*****************************************************************************/
/* returns error */
int
scard_process_begin_transaction(struct trans *con, struct stream *in_s)
{
    int hCard;
    struct pcsc_uds_client *uds_client;
    void *user_data;
    struct pcsc_card *lcard;
    struct pcsc_context *lcontext;

    LLOGLN(10, ("scard_process_begin_transaction:"));
    uds_client = (struct pcsc_uds_client *) (con->callback_data);
    in_uint32_le(in_s, hCard);
    LLOGLN(10, ("scard_process_begin_transaction: hCard 0x%8.8x", hCard));
    user_data = (void *) (tintptr) (uds_client->uds_client_id);
    lcard = get_pcsc_card_by_app_card(uds_client, hCard, &lcontext);
    if ((lcard == 0) || (lcontext == 0))
    {
        LLOGLN(0, ("scard_process_begin_transaction: "
               "get_pcsc_card_by_app_card failed"));
        return 1;
    }
    scard_send_begin_transaction(user_data,
                                 lcontext->context, lcontext->context_bytes,
                                 lcard->card, lcard->card_bytes);
    return 0;
}

/*****************************************************************************/
/* returns error */
int
scard_function_begin_transaction_return(void *user_data,
                                        struct stream *in_s,
                                        int len, int status)
{
    struct stream *out_s;
    int bytes;
    int uds_client_id;
    struct pcsc_uds_client *uds_client;
    struct trans *con;

    LLOGLN(10, ("scard_function_begin_transaction_return:"));
    LLOGLN(10, ("  status 0x%8.8x", status));
    uds_client_id = (int) (tintptr) user_data;
    uds_client = (struct pcsc_uds_client *)
                 get_uds_client_by_id(uds_client_id);
    if (uds_client == 0)
    {
        LLOGLN(0, ("scard_function_begin_transaction_return: "
               "get_uds_client_by_id failed"));
        return 1;
    }
    con = uds_client->con;
    out_s = trans_get_out_s(con, 8192);
    s_push_layer(out_s, iso_hdr, 8);
    out_uint32_le(out_s, status); /* SCARD_S_SUCCESS status */
    s_mark_end(out_s);
    bytes = (int) (out_s->end - out_s->data);
    s_pop_layer(out_s, iso_hdr);
    out_uint32_le(out_s, bytes - 8);
    out_uint32_le(out_s, 0x07); /* SCARD_BEGIN_TRANSACTION 0x07 */
    return trans_force_write(con);
}

/*****************************************************************************/
/* returns error */
int
scard_process_end_transaction(struct trans *con, struct stream *in_s)
{
    int hCard;
    int dwDisposition;
    struct pcsc_uds_client *uds_client;
    void *user_data;
    struct pcsc_card *lcard;
    struct pcsc_context *lcontext;

    LLOGLN(10, ("scard_process_end_transaction:"));
    uds_client = (struct pcsc_uds_client *) (con->callback_data);
    in_uint32_le(in_s, hCard);
    in_uint32_le(in_s, dwDisposition);
    LLOGLN(10, ("scard_process_end_transaction: hCard 0x%8.8x", hCard));
    user_data = (void *) (tintptr) (uds_client->uds_client_id);
    lcard = get_pcsc_card_by_app_card(uds_client, hCard, &lcontext);
    if ((lcard == 0) || (lcontext == 0))
    {
        LLOGLN(0, ("scard_process_end_transaction: "
               "get_pcsc_card_by_app_card failed"));
        return 1;
    }
    scard_send_end_transaction(user_data,
                               lcontext->context, lcontext->context_bytes,
                               lcard->card, lcard->card_bytes,
                               dwDisposition);
    return 0;
}

/*****************************************************************************/
/* returns error */
int
scard_function_end_transaction_return(void *user_data,
                                      struct stream *in_s,
                                      int len, int status)
{
    struct stream *out_s;
    int bytes;
    int uds_client_id;
    struct pcsc_uds_client *uds_client;
    struct trans *con;

    LLOGLN(10, ("scard_function_end_transaction_return:"));
    LLOGLN(10, ("  status 0x%8.8x", status));
    uds_client_id = (int) (tintptr) user_data;
    uds_client = (struct pcsc_uds_client *)
                 get_uds_client_by_id(uds_client_id);
    if (uds_client == 0)
    {
        LLOGLN(0, ("scard_function_end_transaction_return: "
               "get_uds_client_by_id failed"));
        return 1;
    }
    con = uds_client->con;

    out_s = trans_get_out_s(con, 8192);
    s_push_layer(out_s, iso_hdr, 8);
    out_uint32_le(out_s, status); /* SCARD_S_SUCCESS status */
    s_mark_end(out_s);
    bytes = (int) (out_s->end - out_s->data);
    s_pop_layer(out_s, iso_hdr);
    out_uint32_le(out_s, bytes - 8);
    out_uint32_le(out_s, 0x08); /* SCARD_END_TRANSACTION 0x08 */
    return trans_force_write(con);
}

/*****************************************************************************/
/* returns error */
int
scard_function_get_attrib_return(void *user_data,
                                 struct stream *in_s,
                                 int len, int status)
{
    return 0;
}

/*****************************************************************************/
struct pcsc_transmit
{
    int uds_client_id;
    struct xrdp_scard_io_request recv_ior;
    int cbRecvLength;
};

/*****************************************************************************/
/* returns error */
int
scard_process_transmit(struct trans *con, struct stream *in_s)
{
    int hCard;
    int recv_bytes;
    int send_bytes;
    char *send_data;
    struct xrdp_scard_io_request send_ior;
    struct xrdp_scard_io_request recv_ior;
    struct pcsc_uds_client *uds_client;
    struct pcsc_card *lcard;
    struct pcsc_context *lcontext;
    struct pcsc_transmit *pcscTransmit;

    LLOGLN(10, ("scard_process_transmit:"));
    uds_client = (struct pcsc_uds_client *) (con->callback_data);
    LLOGLN(10, ("scard_process_transmit:"));
    in_uint32_le(in_s, hCard);
    in_uint32_le(in_s, send_ior.dwProtocol);
    in_uint32_le(in_s, send_ior.cbPciLength);
    in_uint32_le(in_s, send_ior.extra_bytes);
    in_uint8p(in_s, send_ior.extra_data, send_ior.extra_bytes);
    in_uint32_le(in_s, send_bytes);
    in_uint8p(in_s, send_data, send_bytes);
    in_uint32_le(in_s, recv_ior.dwProtocol);
    in_uint32_le(in_s, recv_ior.cbPciLength);
    in_uint32_le(in_s, recv_ior.extra_bytes);
    in_uint8p(in_s, recv_ior.extra_data, recv_ior.extra_bytes);
    in_uint32_le(in_s, recv_bytes);
    LLOGLN(10, ("scard_process_transmit: send dwProtocol %d cbPciLength %d "
           "recv dwProtocol %d cbPciLength %d send_bytes %d ",
           send_ior.dwProtocol, send_ior.cbPciLength, recv_ior.dwProtocol,
           recv_ior.cbPciLength, send_bytes));
    LLOGLN(10, ("scard_process_transmit: recv_bytes %d", recv_bytes));
    lcard = get_pcsc_card_by_app_card(uds_client, hCard, &lcontext);
    if ((lcard == 0) || (lcontext == 0))
    {
        LLOGLN(0, ("scard_process_transmit: "
               "get_pcsc_card_by_app_card failed"));
        return 1;
    }

    pcscTransmit = (struct pcsc_transmit *)
                   g_malloc(sizeof(struct pcsc_transmit), 1);
    pcscTransmit->uds_client_id = uds_client->uds_client_id;
    pcscTransmit->recv_ior = recv_ior;
    pcscTransmit->cbRecvLength = recv_bytes;

    scard_send_transmit(pcscTransmit,
                        lcontext->context, lcontext->context_bytes,
                        lcard->card, lcard->card_bytes,
                        send_data, send_bytes, recv_bytes,
                        &send_ior, &recv_ior);
    return 0;
}

/*****************************************************************************/
/* returns error */
int
scard_function_transmit_return(void *user_data,
                               struct stream *in_s,
                               int len, int status)
{
    struct stream *out_s;
    int bytes;
    int val;
    int cbRecvLength;
    char *recvBuf;
    struct xrdp_scard_io_request recv_ior;
    struct pcsc_uds_client *uds_client;
    struct trans *con;
    struct pcsc_transmit *pcscTransmit;

    LLOGLN(10, ("scard_function_transmit_return:"));
    LLOGLN(10, ("  status 0x%8.8x", status));
    pcscTransmit = (struct pcsc_transmit *) user_data;
    recv_ior = pcscTransmit->recv_ior;
    uds_client = (struct pcsc_uds_client *)
                 get_uds_client_by_id(pcscTransmit->uds_client_id);
    g_free(pcscTransmit);

    if (uds_client == 0)
    {
        LLOGLN(0, ("scard_function_transmit_return: "
               "get_uds_client_by_id failed"));
        return 1;
    }
    con = uds_client->con;
    cbRecvLength = 0;
    recvBuf = 0;
    if (status == 0)
    {
        in_uint8s(in_s, 20);
        in_uint32_le(in_s, val);
        if (val != 0)
        {
            /* pioRecvPci */
            in_uint8s(in_s, 8);
            in_uint32_le(in_s, recv_ior.dwProtocol);
            in_uint32_le(in_s, recv_ior.cbPciLength);
            recv_ior.cbPciLength += 8;
            in_uint32_le(in_s, recv_ior.extra_bytes);
            if (recv_ior.extra_bytes > 0)
            {
                in_uint8p(in_s, recv_ior.extra_data, recv_ior.extra_bytes);
            }
        }

        in_uint8s(in_s, 4);
        in_uint32_le(in_s, val);
        if (val != 0)
        {
            in_uint32_le(in_s, cbRecvLength);
            in_uint8p(in_s, recvBuf, cbRecvLength);
        }

    }
    LLOGLN(10, ("scard_function_transmit_return: cbRecvLength %d", cbRecvLength));
    out_s = trans_get_out_s(con, 8192);
    s_push_layer(out_s, iso_hdr, 8);
    out_uint32_le(out_s, recv_ior.dwProtocol);
    out_uint32_le(out_s, recv_ior.cbPciLength);
    out_uint32_le(out_s, recv_ior.extra_bytes);
    out_uint8a(out_s, recv_ior.extra_data, recv_ior.extra_bytes);
    out_uint32_le(out_s, cbRecvLength);
    out_uint8a(out_s, recvBuf, cbRecvLength);
    out_uint32_le(out_s, status); /* SCARD_S_SUCCESS status */
    s_mark_end(out_s);
    bytes = (int) (out_s->end - out_s->data);
    s_pop_layer(out_s, iso_hdr);
    out_uint32_le(out_s, bytes - 8);
    out_uint32_le(out_s, 0x09); /* SCARD_TRANSMIT 0x09 */
    return trans_force_write(con);
}

/*****************************************************************************/
/* returns error */
int
scard_process_control(struct trans *con, struct stream *in_s)
{
    int hCard;
    int send_bytes;
    int recv_bytes;
    int control_code;
    char *send_data;
    struct pcsc_uds_client *uds_client;
    void *user_data;
    struct pcsc_context *lcontext;
    struct pcsc_card *lcard;

    LLOGLN(10, ("scard_process_control:"));
    uds_client = (struct pcsc_uds_client *) (con->callback_data);
    LLOGLN(10, ("scard_process_control:"));

    in_uint32_le(in_s, hCard);
    in_uint32_le(in_s, control_code);
    in_uint32_le(in_s, send_bytes);
    in_uint8p(in_s, send_data, send_bytes);
    in_uint32_le(in_s, recv_bytes);

    user_data = (void *) (tintptr) (uds_client->uds_client_id);
    lcard = get_pcsc_card_by_app_card(uds_client, hCard, &lcontext);
    if ((lcard == 0) || (lcontext == 0))
    {
        LLOGLN(0, ("scard_process_control: "
               "get_pcsc_card_by_app_card failed"));
        return 1;
    }
    scard_send_control(user_data, lcontext->context, lcontext->context_bytes,
                       lcard->card, lcard->card_bytes,
                       send_data, send_bytes, recv_bytes,
                       control_code);

    return 0;
}

/*****************************************************************************/
/* returns error */
int
scard_function_control_return(void *user_data,
                              struct stream *in_s,
                              int len, int status)
{
    struct stream *out_s;
    int bytes;
    int cbRecvLength;
    char *recvBuf;
    int uds_client_id;
    struct pcsc_uds_client *uds_client;
    struct trans *con;

    LLOGLN(10, ("scard_function_control_return:"));
    LLOGLN(10, ("  status 0x%8.8x", status));
    uds_client_id = (int) (tintptr) user_data;
    uds_client = (struct pcsc_uds_client *)
                 get_uds_client_by_id(uds_client_id);
    if (uds_client == 0)
    {
        LLOGLN(0, ("scard_function_control_return: "
               "get_uds_client_by_id failed"));
        return 1;
    }
    con = uds_client->con;
    cbRecvLength = 0;
    recvBuf = 0;
    if (status == 0)
    {
        in_uint8s(in_s, 28);
        in_uint32_le(in_s, cbRecvLength);
        in_uint8p(in_s, recvBuf, cbRecvLength);
    }
    LLOGLN(10, ("scard_function_control_return: cbRecvLength %d", cbRecvLength));
    out_s = trans_get_out_s(con, 8192);
    s_push_layer(out_s, iso_hdr, 8);
    out_uint32_le(out_s, cbRecvLength);
    out_uint8a(out_s, recvBuf, cbRecvLength);
    out_uint32_le(out_s, status); /* SCARD_S_SUCCESS status */
    s_mark_end(out_s);
    bytes = (int) (out_s->end - out_s->data);
    s_pop_layer(out_s, iso_hdr);
    out_uint32_le(out_s, bytes - 8);
    out_uint32_le(out_s, 0x0A); /* SCARD_CONTROL 0x0A */
    return trans_force_write(con);
}

/*****************************************************************************/
struct pcsc_status
{
    int uds_client_id;
    int cchReaderLen;
};

/*****************************************************************************/
/* returns error */
int
scard_process_status(struct trans *con, struct stream *in_s)
{
    int hCard;
    int cchReaderLen;
    int cbAtrLen;
    struct pcsc_uds_client *uds_client;
    struct pcsc_card *lcard;
    struct pcsc_context *lcontext;
    struct pcsc_status *pcscStatus;

    LLOGLN(10, ("scard_process_status:"));
    uds_client = (struct pcsc_uds_client *) (con->callback_data);

    in_uint32_le(in_s, hCard);
    in_uint32_le(in_s, cchReaderLen);
    in_uint32_le(in_s, cbAtrLen);
    LLOGLN(10, ("scard_process_status: hCard 0x%8.8x cchReaderLen %d "
           "cbAtrLen %d", hCard, cchReaderLen, cbAtrLen));

    lcard = get_pcsc_card_by_app_card(uds_client, hCard, &lcontext);
    if ((lcard == 0) || (lcontext == 0))
    {
        LLOGLN(0, ("scard_process_status: "
               "get_pcsc_card_by_app_card failed"));
        return 1;
    }
    pcscStatus = (struct pcsc_status *) g_malloc(sizeof(struct pcsc_status), 1);
    pcscStatus->uds_client_id = uds_client->uds_client_id;
    pcscStatus->cchReaderLen = cchReaderLen;
    scard_send_status(pcscStatus, 1,
                      lcontext->context, lcontext->context_bytes,
                      lcard->card, lcard->card_bytes,
                      cchReaderLen, cbAtrLen);

    return 0;
}

#define MS_SCARD_UNKNOWN    0
#define MS_SCARD_ABSENT     1
#define MS_SCARD_PRESENT    2
#define MS_SCARD_SWALLOWED  3
#define MS_SCARD_POWERED    4
#define MS_SCARD_NEGOTIABLE 5
#define MS_SCARD_SPECIFIC   6

#define PC_SCARD_UNKNOWN    0x0001 /**< Unknown state */
#define PC_SCARD_ABSENT     0x0002 /**< Card is absent */
#define PC_SCARD_PRESENT    0x0004 /**< Card is present */
#define PC_SCARD_SWALLOWED  0x0008 /**< Card not powered */
#define PC_SCARD_POWERED    0x0010 /**< Card is powered */
#define PC_SCARD_NEGOTIABLE 0x0020 /**< Ready for PTS */
#define PC_SCARD_SPECIFIC   0x0040 /**< PTS has been set */

static int g_ms2pc[] = { PC_SCARD_UNKNOWN, PC_SCARD_ABSENT,
                         PC_SCARD_PRESENT, PC_SCARD_SWALLOWED,
                         PC_SCARD_POWERED, PC_SCARD_NEGOTIABLE,
                         PC_SCARD_SPECIFIC };

/*****************************************************************************/
/* returns error */
int
scard_function_status_return(void *user_data,
                             struct stream *in_s,
                             int len, int status)
{
    struct stream *out_s;
    int index;
    int bytes;
    int dwReaderLen;
    int dwState;
    int dwProtocol;
    int dwAtrLen;
    char attr[32];
    twchar reader_name[100];
    char lreader_name[100];
    int uds_client_id;
    struct pcsc_uds_client *uds_client;
    struct trans *con;
    struct pcsc_status *pcscStatus;

    LLOGLN(10, ("scard_function_status_return:"));
    LLOGLN(10, ("  status 0x%8.8x", status));

    pcscStatus = (struct pcsc_status *) user_data;
    if (pcscStatus == 0)
    {
        LLOGLN(0, ("scard_function_status_return: pcscStatus is nil"));
        return 1;
    }

    uds_client_id = pcscStatus->uds_client_id;
    uds_client = (struct pcsc_uds_client *)
                 get_uds_client_by_id(uds_client_id);
    if (uds_client == 0)
    {
        LLOGLN(0, ("scard_function_status_return: "
               "get_uds_client_by_id failed"));
        g_free(pcscStatus);
        return 1;
    }
    g_free(pcscStatus);
    con = uds_client->con;
    dwReaderLen = 0;
    dwState = 0;
    dwProtocol = 0;
    dwAtrLen = 0;
    lreader_name[0] = 0;
    if (status == 0)
    {
        in_uint8s(in_s, 20);
        in_uint32_le(in_s, dwReaderLen);
        in_uint8s(in_s, 4);
        in_uint32_le(in_s, dwState);
        dwState = g_ms2pc[dwState % 6];
        in_uint32_le(in_s, dwProtocol);
        in_uint8a(in_s, attr, 32);
        in_uint32_le(in_s, dwAtrLen);
        if (dwReaderLen > 0)
        {
            in_uint32_le(in_s, dwReaderLen);
            dwReaderLen /= 2;
        }
        else
        {
            dwReaderLen = 1;
        }
        if (dwReaderLen < 1)
        {
            LLOGLN(0, ("scard_function_status_return: dwReaderLen < 1"));
            dwReaderLen = 1;
        }
        if (dwReaderLen > 99)
        {
            LLOGLN(0, ("scard_function_status_return: dwReaderLen too big "
                   "0x%8.8x", dwReaderLen));
            dwReaderLen = 99;
        }
        g_memset(reader_name, 0, sizeof(reader_name));
        g_memset(lreader_name, 0, sizeof(lreader_name));
        for (index = 0; index < dwReaderLen - 1; index++)
        {
            in_uint16_le(in_s, reader_name[index]);
        }
        g_wcstombs(lreader_name, reader_name, 99);
    }
    LLOGLN(10, ("scard_function_status_return: dwAtrLen %d dwReaderLen %d "
           "dwProtocol %d dwState %d name %s",
           dwAtrLen, dwReaderLen, dwProtocol, dwState, lreader_name));
    out_s = trans_get_out_s(con, 8192);
    s_push_layer(out_s, iso_hdr, 8);
    dwReaderLen = g_strlen(lreader_name);
    out_uint32_le(out_s, dwReaderLen);
    out_uint8a(out_s, lreader_name, dwReaderLen);
    out_uint32_le(out_s, dwState);
    out_uint32_le(out_s, dwProtocol);
    out_uint32_le(out_s, dwAtrLen);
    out_uint8a(out_s, attr, dwAtrLen);
    out_uint32_le(out_s, status); /* SCARD_S_SUCCESS status */
    s_mark_end(out_s);
    bytes = (int) (out_s->end - out_s->data);
    s_pop_layer(out_s, iso_hdr);
    out_uint32_le(out_s, bytes - 8);
    out_uint32_le(out_s, 0x0B); /* SCARD_STATUS 0x0B */
    return trans_force_write(con);
}

/*****************************************************************************/
/* returns error */
int
scard_process_get_status_change(struct trans *con, struct stream *in_s)
{
    int index;
    int hContext;
    int dwTimeout;
    int cReaders;
    READER_STATE *rsa;
    struct pcsc_uds_client *uds_client;
    void *user_data;
    struct pcsc_context *lcontext;

    LLOGLN(10, ("scard_process_get_status_change:"));
    uds_client = (struct pcsc_uds_client *) (con->callback_data);
    in_uint32_le(in_s, hContext);
    in_uint32_le(in_s, dwTimeout);
    in_uint32_le(in_s, cReaders);
    if ((cReaders < 0) || (cReaders > 16))
    {
        LLOGLN(0, ("scard_process_get_status_change: bad cReaders %d", cReaders));
        return 1;
    }
    rsa = (READER_STATE *) g_malloc(sizeof(READER_STATE) * cReaders, 1);

    for (index = 0; index < cReaders; index++)
    {
        in_uint8a(in_s, rsa[index].reader_name, 100);
        LLOGLN(10, ("scard_process_get_status_change: reader_name %s",
               rsa[index].reader_name));
        in_uint32_le(in_s, rsa[index].current_state);
        LLOGLN(10, ("scard_process_get_status_change: current_state %d",
               rsa[index].current_state));
        in_uint32_le(in_s, rsa[index].event_state);
        LLOGLN(10, ("scard_process_get_status_change: event_state %d",
               rsa[index].event_state));
        in_uint32_le(in_s, rsa[index].atr_len);
        LLOGLN(10, ("scard_process_get_status_change: atr_len %d",
               rsa[index].atr_len));
        in_uint8a(in_s, rsa[index].atr, 36);
    }

    LLOGLN(10, ("scard_process_get_status_change: hContext 0x%8.8x dwTimeout "
           "%d cReaders %d", hContext, dwTimeout, cReaders));

    user_data = (void *) (tintptr) (uds_client->uds_client_id);
    lcontext = get_pcsc_context_by_app_context(uds_client, hContext);
    if (lcontext == 0)
    {
        LLOGLN(0, ("scard_process_get_status_change: "
               "get_pcsc_context_by_app_context failed"));
        g_free(rsa);
        return 1;
    }
    scard_send_get_status_change(user_data,
                                 lcontext->context, lcontext->context_bytes,
                                 1, dwTimeout, cReaders, rsa);
    g_free(rsa);

    return 0;
}

/*****************************************************************************/
int
scard_function_get_status_change_return(void *user_data,
                                        struct stream *in_s,
                                        int len, int status)
{
    int bytes;
    int index;
    int cReaders;
    tui32 current_state;
    tui32 event_state;
    tui32 atr_len; /* number of bytes in atr[] */
    tui8 atr[36];
    struct stream *out_s;
    int uds_client_id;
    struct pcsc_uds_client *uds_client;
    struct trans *con;

    LLOGLN(10, ("scard_function_get_status_change_return:"));
    LLOGLN(10, ("  status 0x%8.8x", status));
    uds_client_id = (int) (tintptr) user_data;
    uds_client = (struct pcsc_uds_client *)
                 get_uds_client_by_id(uds_client_id);
    if (uds_client == 0)
    {
        LLOGLN(0, ("scard_function_get_status_change_return: "
               "get_uds_client_by_id failed"));
        return 1;
    }
    con = uds_client->con;

    out_s = trans_get_out_s(con, 8192);
    s_push_layer(out_s, iso_hdr, 8);
    if (status != 0)
    {
        out_uint32_le(out_s, 0); /* cReaders */
        out_uint32_le(out_s, status); /* SCARD_S_SUCCESS status */
    }
    else
    {
        in_uint8s(in_s, 28);
        in_uint32_le(in_s, cReaders);
        LLOGLN(10, ("  cReaders %d", cReaders));
        out_uint32_le(out_s, cReaders);
        if (cReaders > 0)
        {
            for (index = 0; index < cReaders; index++)
            {
                in_uint32_le(in_s, current_state);
                out_uint32_le(out_s, current_state);
                in_uint32_le(in_s, event_state);
                out_uint32_le(out_s, event_state);
                in_uint32_le(in_s, atr_len);
                out_uint32_le(out_s, atr_len);
                in_uint8a(in_s, atr, 36);
                out_uint8a(out_s, atr, 36);
            }
        }
        out_uint32_le(out_s, status); /* SCARD_S_SUCCESS status */
    }

    s_mark_end(out_s);
    bytes = (int) (out_s->end - out_s->data);
    s_pop_layer(out_s, iso_hdr);
    out_uint32_le(out_s, bytes - 8);
    out_uint32_le(out_s, 0x0C); /* SCARD_ESTABLISH_CONTEXT 0x0C */
    return trans_force_write(con);
}

/*****************************************************************************/
/* returns error */
int
scard_process_cancel(struct trans *con, struct stream *in_s)
{
    int hContext;
    struct pcsc_uds_client *uds_client;
    void *user_data;
    struct pcsc_context *lcontext;

    LLOGLN(10, ("scard_process_cancel:"));
    uds_client = (struct pcsc_uds_client *) (con->callback_data);
    in_uint32_le(in_s, hContext);
    LLOGLN(10, ("scard_process_cancel: hContext 0x%8.8x", hContext));
    user_data = (void *) (tintptr) (uds_client->uds_client_id);
    lcontext = get_pcsc_context_by_app_context(uds_client, hContext);
    if (lcontext == 0)
    {
        LLOGLN(0, ("scard_process_cancel: "
               "get_pcsc_context_by_app_context failed"));
        return 1;
    }
    scard_send_cancel(user_data, lcontext->context, lcontext->context_bytes);
    return 0;
}

/*****************************************************************************/
/* returns error */
int
scard_function_cancel_return(void *user_data,
                             struct stream *in_s,
                             int len, int status)
{
    int bytes;
    int uds_client_id;
    struct stream *out_s;
    struct pcsc_uds_client *uds_client;
    struct trans *con;

    LLOGLN(10, ("scard_function_cancel_return:"));
    LLOGLN(10, ("  status 0x%8.8x", status));
    uds_client_id = (int) (tintptr) user_data;
    uds_client = (struct pcsc_uds_client *)
                 get_uds_client_by_id(uds_client_id);
    if (uds_client == 0)
    {
        LLOGLN(0, ("scard_function_cancel_return: "
               "get_uds_client_by_id failed"));
        return 1;
    }
    con = uds_client->con;
    out_s = trans_get_out_s(con, 8192);
    s_push_layer(out_s, iso_hdr, 8);
    out_uint32_le(out_s, status); /* SCARD_S_SUCCESS status */
    s_mark_end(out_s);
    bytes = (int) (out_s->end - out_s->data);
    s_pop_layer(out_s, iso_hdr);
    out_uint32_le(out_s, bytes - 8);
    out_uint32_le(out_s, 0x0D); /* SCARD_CANCEL 0x0D */
    return trans_force_write(con);
}

/*****************************************************************************/
/* returns error */
int
scard_function_is_context_valid_return(void *user_data,
                                       struct stream *in_s,
                                       int len, int status)
{
    return 0;
}

/*****************************************************************************/
/* returns error */
int scard_function_reconnect_return(void *user_data,
                                           struct stream *in_s,
                                           int len, int status)
{
    return 0;
}

/*****************************************************************************/
/* returns error */
int
scard_process_msg(struct trans *con, struct stream *in_s, int command)
{
    int rv;

    LLOGLN(10, ("scard_process_msg: command 0x%4.4x", command));
    rv = 0;
    switch (command)
    {
        case 0x01: /* SCARD_ESTABLISH_CONTEXT */
            LLOGLN(10, ("scard_process_msg: SCARD_ESTABLISH_CONTEXT"));
            rv = scard_process_establish_context(con, in_s);
            break;
        case 0x02: /* SCARD_RELEASE_CONTEXT */
            LLOGLN(10, ("scard_process_msg: SCARD_RELEASE_CONTEXT"));
            rv = scard_process_release_context(con, in_s);
            break;

        case 0x03: /* SCARD_LIST_READERS */
            LLOGLN(10, ("scard_process_msg: SCARD_LIST_READERS"));
            rv = scard_process_list_readers(con, in_s);
            break;

        case 0x04: /* SCARD_CONNECT */
            LLOGLN(10, ("scard_process_msg: SCARD_CONNECT"));
            rv = scard_process_connect(con, in_s);
            break;

        case 0x05: /* SCARD_RECONNECT */
            LLOGLN(0, ("scard_process_msg: SCARD_RECONNECT"));
            break;

        case 0x06: /* SCARD_DISCONNECT */
            LLOGLN(10, ("scard_process_msg: SCARD_DISCONNECT"));
            rv = scard_process_disconnect(con, in_s);
            break;

        case 0x07: /* SCARD_BEGIN_TRANSACTION */
            LLOGLN(10, ("scard_process_msg: SCARD_BEGIN_TRANSACTION"));
            rv = scard_process_begin_transaction(con, in_s);
            break;

        case 0x08: /* SCARD_END_TRANSACTION */
            LLOGLN(10, ("scard_process_msg: SCARD_END_TRANSACTION"));
            rv = scard_process_end_transaction(con, in_s);
            break;

        case 0x09: /* SCARD_TRANSMIT */
            LLOGLN(10, ("scard_process_msg: SCARD_TRANSMIT"));
            rv = scard_process_transmit(con, in_s);
            break;

        case 0x0A: /* SCARD_CONTROL */
            LLOGLN(10, ("scard_process_msg: SCARD_CONTROL"));
            rv = scard_process_control(con, in_s);
            break;

        case 0x0B: /* SCARD_STATUS */
            LLOGLN(10, ("scard_process_msg: SCARD_STATUS"));
            rv = scard_process_status(con, in_s);
            break;

        case 0x0C: /* SCARD_GET_STATUS_CHANGE */
            LLOGLN(10, ("scard_process_msg: SCARD_GET_STATUS_CHANGE"));
            rv = scard_process_get_status_change(con, in_s);
            break;

        case 0x0D: /* SCARD_CANCEL */
            LLOGLN(10, ("scard_process_msg: SCARD_CANCEL"));
            rv = scard_process_cancel(con, in_s);
            break;

        case 0x0E: /* SCARD_CANCEL_TRANSACTION */
            LLOGLN(0, ("scard_process_msg: SCARD_CANCEL_TRANSACTION"));
            break;

        case 0x0F: /* SCARD_GET_ATTRIB */
            LLOGLN(0, ("scard_process_msg: SCARD_GET_ATTRIB"));
            break;

        case 0x10: /* SCARD_SET_ATTRIB */
            LLOGLN(0, ("scard_process_msg: SCARD_SET_ATTRIB"));
            break;

        default:
            LLOGLN(0, ("scard_process_msg: unknown mtype 0x%4.4x", command));
            rv = 1;
            break;
    }
    return rv;
}

/*****************************************************************************/
/* returns error */
int
my_pcsc_trans_data_in(struct trans *trans)
{
    struct stream *s;
    int size;
    int command;
    int error;

    LLOGLN(10, ("my_pcsc_trans_data_in:"));
    if (trans == 0)
    {
        return 0;
    }
    s = trans_get_in_s(trans);
    in_uint32_le(s, size);
    in_uint32_le(s, command);
    LLOGLN(10, ("my_pcsc_trans_data_in: size %d command %d", size, command));
    error = trans_force_read(trans, size);
    if (error == 0)
    {
        error = scard_process_msg(trans, s, command);
    }
    return error;
}

/*****************************************************************************/
/* got a new connection from libpcsclite */
int
my_pcsc_trans_conn_in(struct trans *trans, struct trans *new_trans)
{
    struct pcsc_uds_client *uds_client;

    LLOGLN(10, ("my_pcsc_trans_conn_in:"));

    if (trans == 0)
    {
        return 1;
    }

    if (trans != g_lis)
    {
        return 1;
    }

    if (new_trans == 0)
    {
        return 1;
    }

    uds_client = create_uds_client(new_trans);
    if (uds_client == 0)
    {
        return 1;
    }
    uds_client->con->trans_data_in = my_pcsc_trans_data_in;
    uds_client->con->header_size = 8;

    if (g_uds_clients == 0)
    {
        g_uds_clients = list_create();
    }
    list_add_item(g_uds_clients, (tbus)uds_client);

    return 0;
}

/*****************************************************************************/
int
scard_pcsc_init(void)
{
    char *home;
    int disp;
    int error;

    LLOGLN(0, ("scard_pcsc_init:"));
    if (g_lis == 0)
    {
        g_lis = trans_create(2, 8192, 8192);
        home = g_getenv("HOME");
        disp = g_display_num;
        g_snprintf(g_pcsclite_ipc_dir, 255, "%s/.pcsc%d", home, disp);

        if (g_directory_exist(g_pcsclite_ipc_dir))
        {
            if (!g_remove_dir(g_pcsclite_ipc_dir))
            {
                LLOGLN(0, ("scard_pcsc_init: g_remove_dir failed"));
            }
        }
        if (!g_directory_exist(g_pcsclite_ipc_dir))
        {
            if (!g_create_dir(g_pcsclite_ipc_dir))
            {
                if (!g_directory_exist(g_pcsclite_ipc_dir))
                {
                    LLOGLN(0, ("scard_pcsc_init: g_create_dir failed"));
                }
            }
        }
        g_chmod_hex(g_pcsclite_ipc_dir, 0x1777);
        g_snprintf(g_pcsclite_ipc_file, 255, "%s/pcscd.comm", g_pcsclite_ipc_dir);
        g_lis->trans_conn_in = my_pcsc_trans_conn_in;
        error = trans_listen(g_lis, g_pcsclite_ipc_file);
        if (error != 0)
        {
            LLOGLN(0, ("scard_pcsc_init: trans_listen failed for port %s",
                   g_pcsclite_ipc_file));
            return 1;
        }
    }
    return 0;
}

/*****************************************************************************/
int
scard_pcsc_deinit(void)
{
    LLOGLN(0, ("scard_pcsc_deinit:"));

    if (g_lis != 0)
    {
        trans_delete(g_lis);
        g_lis = 0;
    }

    if (g_pcsclite_ipc_dir[0] != 0)
    {
        g_file_delete(g_pcsclite_ipc_file);
        if (!g_remove_dir(g_pcsclite_ipc_dir))
        {
            LLOGLN(0, ("scard_pcsc_deinit: g_remove_dir failed"));
        }
        g_pcsclite_ipc_dir[0] = 0;
    }

    return 0;
}

#else

int
scard_pcsc_get_wait_objs(tbus *objs, int *count, int *timeout)
{
    return 0;
}
int
scard_pcsc_check_wait_objs(void)
{
    return 0;
}
int
scard_pcsc_init(void)
{
    return 0;
}
int
scard_pcsc_deinit(void)
{
    return 0;
}

#endif
