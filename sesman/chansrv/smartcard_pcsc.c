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
#include "defines.h"

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

typedef struct pubReaderStatesList
{
    char readerName[128];
    tui32 eventCounter;
    tui32 readerState;
    tui32 readerSharing;
    tui8 cardAtr[36];
    tui32 cardAtrLength;
    tui32 cardProtocol;
} PCSC_READER_STATE;

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
    int ref_count;
    struct trans *con;     /* the connection to the app */
    struct list *contexts; /* list of struct pcsc_context */

    struct pcsc_context *connect_context; /* TODO remove this */

    int state;
    int pad1;

    /* should be 2944 bytes */
    /* 128 + 4 + 4 + 4 + 36 + 4 + 4 = 186, 186 * 16 = 2944 */
    PCSC_READER_STATE readerStates[16];
    tui32 current_states[16];
    tui32 event_states[16];
    int numReaders;
    int waiting;
    int something_changed;
    int send_status;
};

static struct list *g_uds_clients = 0; /* struct pcsc_uds_client */

static struct trans *g_lis = 0;
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
get_first_pcsc_context(struct pcsc_uds_client *uds_client)
{
    if (uds_client == 0)
    {
        return 0;
    }
    if (uds_client->contexts == 0)
    {
        return 0;
    }
    return (struct pcsc_context *) list_get_item(uds_client->contexts, 0);
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
    if (uds_client->ref_count > 0)
    {
        return 0;
    }
    LLOGLN(10, ("free_uds_client: freeing"));
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
static int
cancel_uds_client(struct pcsc_uds_client *uds_client)
{
    int i;
    struct pcsc_context *context;
    void *user_data;

    LLOGLN(10, ("cancel_uds_client:"));
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
                user_data = (void *) (tintptr) (uds_client->uds_client_id);
                uds_client->ref_count++;
                scard_send_cancel(user_data, context->context, context->context_bytes);
            }
        }
    }
    return 0;
}

/*****************************************************************************/
static struct pcsc_context *
uds_client_add_context(struct pcsc_uds_client *uds_client,
                       char *context, int context_bytes)
{
    struct pcsc_context *pcscContext;

    LLOGLN(10, ("uds_client_add_context:"));
    pcscContext = g_new0(struct pcsc_context, 1);
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
    pcscCard = g_new0(struct pcsc_card, 1);
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
                    LLOGLN(0, ("scard_pcsc_check_wait_objs: calling free_uds_client"));
                    cancel_uds_client(uds_client);
                    uds_client->ref_count--;
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
    uds_client->ref_count++;
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
    int rv;
    int uds_client_id;
    int context_bytes;
    int app_context;
    int return_code;
    char context[16];
    struct stream *out_s;
    struct pcsc_uds_client *uds_client;
    struct trans *con;
    struct pcsc_context *lcontext;

    LLOGLN(10, ("scard_function_establish_context_return:"));
    LLOGLN(10, ("  status 0x%8.8x", status));
    rv = 0;
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
    return_code = 0;
    g_memset(context, 0, 16);
    if (status == 0)
    {
        in_uint8s(in_s, 16);
        in_uint32_le(in_s, return_code);
        in_uint8s(in_s, 8);
        in_uint32_le(in_s, context_bytes);
        if (return_code != 0 || context_bytes < 4 || context_bytes > 16)
        {
            LLOGLN(0, ("scard_function_establish_context_return: error "
                   "return_code 0x%8.8x context_bytes %d",
                   return_code, context_bytes));
            if (uds_client->state == 1)
            {
                out_s = uds_client->con->out_s;
                init_stream(out_s, 8192);
                out_uint32_le(out_s, 4);
                out_uint32_le(out_s, 3);
                out_uint32_le(out_s, 0x80100001); /* result */
                s_mark_end(out_s);
                rv = trans_write_copy(uds_client->con);
                uds_client->state = 0;
            }
            uds_client->ref_count--;
            free_uds_client(uds_client);
            return rv;
        }
        in_uint8a(in_s, context, context_bytes);
        lcontext = uds_client_add_context(uds_client, context, context_bytes);
        app_context = lcontext->app_context;
        LLOGLN(10, ("scard_function_establish_context_return: "
               "app_context %d", app_context));
    }
    else
    {
        LLOGLN(0, ("scard_function_establish_context_return: error "
               "status %d", status));
        if (uds_client->state == 1)
        {
            out_s = uds_client->con->out_s;
            init_stream(out_s, 8192);
            out_uint32_le(out_s, 4);
            out_uint32_le(out_s, 3);
            out_uint32_le(out_s, 0x80100001); /* result */
            s_mark_end(out_s);
            rv = trans_write_copy(uds_client->con);
            uds_client->state = 0;
        }
        uds_client->ref_count--;
        free_uds_client(uds_client);
        return rv;
    }
    if (uds_client->state == 1)
    {
        uds_client->ref_count++;
        scard_send_list_readers(uds_client,
                                lcontext->context,
                                lcontext->context_bytes,
                                0, 8148, 0, 0);
    }
    else if (uds_client->state == 0)
    {
        out_s = trans_get_out_s(con, 8192);
        init_stream(out_s, 0);
        out_uint32_le(out_s, 0); /* dwScope */
        out_uint32_le(out_s, app_context); /* hContext  */
        out_uint32_le(out_s, return_code); /* rv  */
        s_mark_end(out_s);
        rv = trans_write_copy(con);
    }
    uds_client->ref_count--;
    free_uds_client(uds_client);
    return rv;
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
    uds_client->ref_count++;
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
    int rv;
    int uds_client_id;
    int return_code;
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
    return_code = 0;
    if (status == 0)
    {
        in_uint8s(in_s, 16);
        in_uint32_le(in_s, return_code);
    }
    out_s = trans_get_out_s(con, 8192);
    init_stream(out_s, 0);
    out_uint32_le(out_s, 0); /* hContext */
    out_uint32_le(out_s, return_code); /* rv */
    s_mark_end(out_s);
    rv = trans_write_copy(con);
    uds_client->ref_count--;
    free_uds_client(uds_client);
    return rv;
}

/*****************************************************************************/
/* returns error */
int
scard_process_list_readers(struct trans *con, struct stream *in_s)
{
    return 0;
}

/*****************************************************************************/
static int
scard_readers_to_list(struct pcsc_uds_client *uds_client,
                      const char *names, int names_bytes)
{
    int reader_index;
    int name_index;
    int names_index;
    char ch;
    PCSC_READER_STATE hold_reader;

    reader_index = 0;
    name_index = 0;
    names_index = 0;
    hold_reader = uds_client->readerStates[reader_index];
    while (names_index < names_bytes)
    {
        ch = names[names_index];
        if (ch == 0)
        {
            if (name_index < 1)
            {
                break;
            }
            uds_client->readerStates[reader_index].readerName[name_index] = 0;
            LLOGLN(10, ("scard_readers_to_list: name [%s]", uds_client->readerStates[reader_index].readerName));
            /* clear if name changes */
            if (g_strcmp(hold_reader.readerName, uds_client->readerStates[reader_index].readerName) != 0)
            {
                uds_client->something_changed = 1;
                LLOGLN(10, ("scard_readers_to_list: name changed, clearing"));
                uds_client->readerStates[reader_index].eventCounter = 0;
                uds_client->readerStates[reader_index].readerState = 0;
                uds_client->readerStates[reader_index].readerSharing = 0;
                g_memset(uds_client->readerStates[reader_index].cardAtr, 0, 36);
                uds_client->readerStates[reader_index].cardAtrLength = 0;
                uds_client->readerStates[reader_index].cardProtocol = 0;
                uds_client->current_states[reader_index] = 0;
                uds_client->event_states[reader_index] = 0;
            }
            reader_index++; 
            hold_reader = uds_client->readerStates[reader_index];
            if (reader_index > 15)
            {
                return 0;
            }
            name_index = 0;
        }
        else
        {
            uds_client->readerStates[reader_index].readerName[name_index] = ch;
            name_index++;
        }
        names_index++;
    }
    uds_client->numReaders = reader_index;
    /* clear the rest */
    while (reader_index < 16)
    {
        g_memset(uds_client->readerStates + reader_index, 0, sizeof(PCSC_READER_STATE));
        uds_client->current_states[reader_index] = 0;
        uds_client->event_states[reader_index] = 0;
        reader_index++;
    }
    return 0;
}

/*****************************************************************************/
int
scard_function_list_readers_return(void *user_data,
                                   struct stream *in_s,
                                   int len, int status)
{
    int return_code;
    int llen;
    int rv;
    int cardAtrLength;
    struct pcsc_uds_client *uds_client;
    struct pcsc_context *context;

    int index;
    READER_STATE *rsa;

    LLOGLN(10, ("scard_function_list_readers_return:"));
    uds_client = (struct pcsc_uds_client *) user_data;
    rv = 0;
    if (status == 0)
    {
        context = get_first_pcsc_context(uds_client);
        if (context != 0)
        {
            in_uint8s(in_s, 16);
            in_uint32_le(in_s, return_code);
            LLOGLN(10, ("scard_function_list_readers_return: return_code 0x%8.8x", return_code));

            rsa = g_new0(READER_STATE, 32);
            in_uint8s(in_s, 4);
            in_uint32_le(in_s, llen); /* pointer use as boolean */
            if ((llen != 0) && (return_code == 0))
            {
                in_uint32_le(in_s, llen);
                LLOGLN(10, ("scard_function_list_readers_return: llen %d", llen));
                //g_hexdump(in_s->p, llen);
                scard_readers_to_list(uds_client, in_s->p, llen);
                for (index = 0; index < uds_client->numReaders; index++)
                {
                    g_strncpy(rsa[index].reader_name, uds_client->readerStates[index].readerName, 127);
                    LLOGLN(10, ("scard_function_list_readers_return: reader for scard_send_get_status_change [%s]",
                           uds_client->readerStates[index].readerName));
                    rsa[index].current_state = uds_client->event_states[index] & ~2;
                    rsa[index].event_state = uds_client->event_states[index];
                    cardAtrLength = MIN(33, uds_client->readerStates[index].cardAtrLength);
                    cardAtrLength = MAX(0, cardAtrLength);
                    g_memcpy(rsa[index].atr, uds_client->readerStates[index].cardAtr, cardAtrLength);
                    rsa[index].atr_len = cardAtrLength;
                }
            }
            else
            {
                LLOGLN(10, ("scard_function_list_readers_return: llen zero"));
                uds_client->numReaders = 0;
                g_memset(uds_client->readerStates, 0, sizeof(uds_client->readerStates));
                g_memset(uds_client->current_states, 0, sizeof(uds_client->current_states));
                g_memset(uds_client->event_states, 0, sizeof(uds_client->event_states));
            }
            /* add plug and play notifier */
            g_strncpy(rsa[uds_client->numReaders].reader_name, "\\\\?PnP?\\Notification", 127);
            rsa[uds_client->numReaders].current_state = uds_client->numReaders << 16;
            rsa[uds_client->numReaders].event_state = 0;
            uds_client->ref_count++;
            scard_send_get_status_change(uds_client,
                                         context->context,
                                         context->context_bytes,
                                         0, 10, uds_client->numReaders + 1, rsa);
            g_free(rsa);
        }
        else
        {
            LLOGLN(0, ("scard_function_list_readers_return: error, no context"));
            rv = 1;
        }
    }
    uds_client->ref_count--;
    free_uds_client(uds_client);
    return rv;
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

    LLOGLN(10, ("scard_process_establish_context:"));
    g_memset(&rs, 0, sizeof(rs));
    uds_client = (struct pcsc_uds_client *) (con->callback_data);
    in_uint32_le(in_s, hContext);
    in_uint8a(in_s, rs.reader_name, 128);
    in_uint32_le(in_s, rs.dwShareMode);
    in_uint32_le(in_s, rs.dwPreferredProtocols);
    LLOGLN(10, ("scard_process_connect: rs.reader_name %s dwShareMode 0x%8.8x "
           "dwPreferredProtocols 0x%8.8x", rs.reader_name, rs.dwShareMode,
           rs.dwPreferredProtocols));
    user_data = (void *) (tintptr) (uds_client->uds_client_id);
    lcontext = get_pcsc_context_by_app_context(uds_client, hContext);
    uds_client->connect_context = lcontext;
    uds_client->ref_count++;
    scard_send_connect(user_data, lcontext->context, lcontext->context_bytes, 0, &rs);
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
    int uds_client_id;
    struct stream *out_s;
    struct pcsc_uds_client *uds_client;
    struct trans *con;
    char *card;
    char dcard[16] = "unknown";
    int return_code;
    struct pcsc_card *lcard;
    int rv;
    int hcontext_bytes;
    int hcontext_present;
    int hcard_bytes;
    int hcard_present;

    LLOGLN(10, ("scard_function_connect_return:"));
    LLOGLN(10, ("  status 0x%8.8x", status));
    rv = 0;
    card = dcard;
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
    return_code = 0;
    if (status == 0)
    {
        in_uint8s(in_s, 16);
        in_uint32_le(in_s, return_code);
        in_uint32_le(in_s, hcontext_bytes);
        in_uint32_le(in_s, hcontext_present);
        in_uint32_le(in_s, hcard_bytes);
        in_uint32_le(in_s, hcard_present);
        LLOGLN(10, ("scard_function_connect_return: hcontext_bytes %d hcard_bytes %d",
               hcontext_bytes, hcard_bytes));
        in_uint32_le(in_s, dwActiveProtocol);
        LLOGLN(10, ("dwActiveProtocol %d", dwActiveProtocol));
        if (hcontext_bytes > 0 && hcontext_present)
        {
            in_uint32_le(in_s, hcontext_bytes);
            in_uint8s(in_s, hcontext_bytes);
        }
        if (hcard_bytes > 0 && hcard_present)
        {
            in_uint32_le(in_s, hcard_bytes);
            in_uint8p(in_s, card, hcard_bytes);
        }
        lcard = context_add_card(uds_client, uds_client->connect_context,
                                 card, hcard_bytes);
        hCard = lcard->app_card;
        LLOGLN(10, ("  hCard %d dwActiveProtocol %d", hCard,
               dwActiveProtocol));
    }
    out_s = trans_get_out_s(con, 8192);
    init_stream(out_s, 0);
    out_uint32_le(out_s, 0); /* hContext */
    out_uint8s(out_s, 128); /* szReader */
    out_uint32_le(out_s, 0); /* dwShareMode */
    out_uint32_le(out_s, 0); /* dwPreferredProtocols */
    out_uint32_le(out_s, hCard);
    out_uint32_le(out_s, dwActiveProtocol);
    out_uint32_le(out_s, return_code); /* rv */
    s_mark_end(out_s);
    rv = trans_write_copy(con);
    uds_client->ref_count--;
    free_uds_client(uds_client);
    return rv;
}

#if 0
struct disconnect_struct
{
    int32_t hCard;
    uint32_t dwDisposition;
    uint32_t rv;
};
#endif

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
    uds_client->ref_count++;
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
    int rv;
    int uds_client_id;
    int return_code;
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
    return_code = 0;
    if (status == 0)
    {
        in_uint8s(in_s, 16);
        in_uint32_le(in_s, return_code);
    }
    out_s = trans_get_out_s(con, 8192);
    init_stream(out_s, 0);
    out_uint32_le(out_s, 0); /* hCard */
    out_uint32_le(out_s, 0); /* dwDisposition */
    out_uint32_le(out_s, return_code); /* rv */
    s_mark_end(out_s);
    rv = trans_write_copy(con);
    uds_client->ref_count--;
    free_uds_client(uds_client);
    return rv;
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
    uds_client->ref_count++;
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
    int rv;
    struct stream *out_s;
    int uds_client_id;
    int return_code;
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
    return_code = 0;
    if (status == 0)
    {
        in_uint8s(in_s, 16);
        in_uint32_le(in_s, return_code);
    }
    out_s = trans_get_out_s(con, 8192);
    init_stream(out_s, 0);
    out_uint32_le(out_s, 0); /* hCard */
    out_uint32_le(out_s, return_code); /* rv */
    s_mark_end(out_s);
    rv = trans_write_copy(con);
    uds_client->ref_count--;
    free_uds_client(uds_client);
    return rv;
}

#if 0
struct end_struct
{
    int32_t hCard;
    uint32_t dwDisposition;
    uint32_t rv;
};
#endif

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
    uds_client->ref_count++;
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
    int rv;
    struct stream *out_s;
    int uds_client_id;
    int return_code;
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
    return_code = 0;
    if (status == 0)
    {
        in_uint8s(in_s, 16);
        in_uint32_le(in_s, return_code);
    }

    out_s = trans_get_out_s(con, 8192);
    init_stream(out_s, 0);
    out_uint32_le(out_s, 0); /* hCard */
    out_uint32_le(out_s, 0); /* dwDisposition */
    out_uint32_le(out_s, return_code); /* rv */
    s_mark_end(out_s);
    rv = trans_write_copy(con);
    uds_client->ref_count--;
    free_uds_client(uds_client);
    return rv;
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
    int recv_ior_is_null;
    int recv_is_null;
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

    recv_ior_is_null = 0;
    recv_is_null = 0;
    g_memset(&send_ior, 0, sizeof(send_ior));
    g_memset(&recv_ior, 0, sizeof(recv_ior));
    in_uint32_le(in_s, hCard);
    in_uint32_le(in_s, send_ior.dwProtocol);
    in_uint32_le(in_s, send_ior.cbPciLength);
    in_uint32_le(in_s, send_bytes);
    in_uint32_le(in_s, recv_ior.dwProtocol);
    in_uint32_le(in_s, recv_ior.cbPciLength);
    in_uint32_le(in_s, recv_bytes);
    in_uint8s(in_s, 4); /* rv */
    in_uint8p(in_s, send_data, send_bytes);
    send_ior.cbPciLength = 8;
    recv_ior.cbPciLength = 8;
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

    pcscTransmit = g_new0(struct pcsc_transmit, 1);
    pcscTransmit->uds_client_id = uds_client->uds_client_id;
    pcscTransmit->recv_ior = recv_ior;
    pcscTransmit->cbRecvLength = recv_bytes;
    uds_client->ref_count++;
    scard_send_transmit(pcscTransmit,
                        lcontext->context, lcontext->context_bytes,
                        lcard->card, lcard->card_bytes,
                        send_data, send_bytes, recv_bytes,
                        &send_ior, &recv_ior, recv_ior_is_null, recv_is_null);
    return 0;
}

/*****************************************************************************/
/* returns error */
int
scard_function_transmit_return(void *user_data,
                               struct stream *in_s,
                               int len, int status)
{
    int rv;
    struct stream *out_s;
    int return_code;
    int cbRecvLength;
    int recv_ior_present;
    int recv_present;
    int recv_ior_extra_present;
    int pad;
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
    return_code = 0;
    if (status == 0)
    {
        in_uint8s(in_s, 16);
        in_uint32_le(in_s, return_code);
        in_uint32_le(in_s, recv_ior_present);
        in_uint32_le(in_s, cbRecvLength);
        in_uint32_le(in_s, recv_present);
        LLOGLN(10, ("scard_function_transmit_return: recv_ior_present %d recv_present %d",
               recv_ior_present, recv_present));
        if (recv_ior_present != 0)
        {
            /* pioRecvPci */
            in_uint32_le(in_s, recv_ior.dwProtocol);
            in_uint32_le(in_s, recv_ior.cbPciLength);
            recv_ior.cbPciLength += 8;
            in_uint32_le(in_s, recv_ior_extra_present);
            if (recv_ior_extra_present != 0)
            {
                in_uint32_le(in_s, recv_ior.extra_bytes);
                if (recv_ior.extra_bytes > 0)
                {
                    in_uint8p(in_s, recv_ior.extra_data, recv_ior.extra_bytes);
                }
                pad = (4 - recv_ior.extra_bytes) & 3;
                in_uint8s(in_s, pad);
            }
        }
        if (recv_present != 0)
        {
            if (s_check_rem(in_s, 4 + cbRecvLength))
            {
                in_uint32_le(in_s, cbRecvLength);
                in_uint8p(in_s, recvBuf, cbRecvLength);
                pad = (4 - cbRecvLength) & 3;
                in_uint8s(in_s, pad);
            }
        }
    }
    LLOGLN(10, ("scard_function_transmit_return: cbRecvLength %d", cbRecvLength));

    out_s = trans_get_out_s(con, 8192 + cbRecvLength);
    out_uint32_le(out_s, 0); /* hCard */
    out_uint32_le(out_s, 0); /* send_ior.dwProtocol */
    out_uint32_le(out_s, 0); /* send_ior.cbPciLength */
    out_uint32_le(out_s, 0); /* send_bytes */
    out_uint32_le(out_s, recv_ior.dwProtocol);
    out_uint32_le(out_s, recv_ior.cbPciLength);
    out_uint32_le(out_s, cbRecvLength);
    out_uint32_le(out_s, return_code); /* rv */
    out_uint8p(out_s, recvBuf, cbRecvLength);
    s_mark_end(out_s);
    rv = trans_write_copy(con);
    uds_client->ref_count--;
    free_uds_client(uds_client);
    return rv;
}

/*****************************************************************************/
/* returns error */
int
scard_process_control(struct trans *con, struct stream *in_s)
{
    return 0;
}

/*****************************************************************************/
/* returns error */
int
scard_function_control_return(void *user_data,
                              struct stream *in_s,
                              int len, int status)
{
    return 0;
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
    int rv;
    int hCard;
    struct pcsc_uds_client *uds_client;
    struct pcsc_card *lcard;
    struct pcsc_context *lcontext;
    struct pcsc_status *pcscStatus;
    struct stream *out_s;

    LLOGLN(10, ("scard_process_status:"));
    rv = 0;
    uds_client = (struct pcsc_uds_client *) (con->callback_data);
    in_uint32_le(in_s, hCard);
    lcard = get_pcsc_card_by_app_card(uds_client, hCard, &lcontext);
    if ((lcard == 0) || (lcontext == 0))
    {
        LLOGLN(0, ("scard_process_status: "
               "get_pcsc_card_by_app_card failed"));
        return 1;
    }
    LLOGLN(10, ("scard_process_status: send_status %d", uds_client->send_status));
    if (uds_client->send_status)
    {
        uds_client->send_status = 0;
        pcscStatus = g_new0(struct pcsc_status, 1);
        pcscStatus->uds_client_id = uds_client->uds_client_id;
        pcscStatus->cchReaderLen = 128;
        uds_client->ref_count++;
        scard_send_status(pcscStatus, 0,
                          lcontext->context, lcontext->context_bytes,
                          lcard->card, lcard->card_bytes,
                          128, 36, 0);
    }
    else
    {
        out_s = con->out_s;
        init_stream(out_s, 0);
        out_uint32_le(out_s, 0); /* hCard */
        out_uint32_le(out_s, 0); /* rv */
        s_mark_end(out_s);
        rv = trans_write_copy(con);
    }
    return rv;
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

    int index;
    int dwReaderLen;
    int dwState;
    int dwProtocol;
    int dwAtrLen;
    int return_code;
    char attr[36];
    char reader_name[128];
    int rv;
    struct stream *out_s;
    struct pcsc_status *pcscStatus;
    int uds_client_id;
    struct pcsc_uds_client *uds_client;
    struct trans *con;

    LLOGLN(10, ("scard_function_status_return:"));
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

    dwReaderLen = 0;
    dwState = 0;
    dwProtocol = 0;
    dwAtrLen = 0;
    reader_name[0] = 0;
    return_code = 0;

    if (status == 0)
    {
        in_uint8s(in_s, 16);
        in_uint32_le(in_s, return_code);
        in_uint32_le(in_s, dwReaderLen);
        in_uint8s(in_s, 4);
        in_uint32_le(in_s, dwState);
        dwState = g_ms2pc[dwState % 7];
        in_uint32_le(in_s, dwProtocol);
        in_uint8a(in_s, attr, 32);
        in_uint32_le(in_s, dwAtrLen);
        LLOGLN(10, ("scard_function_status_return: return_code 0x%8.8x", return_code));
        if (dwReaderLen > 0)
        {
            in_uint8s(in_s, 4);
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
        if (dwReaderLen > 127)
        {
            LLOGLN(10, ("scard_function_status_return: dwReaderLen too big "
                   "0x%8.8x", dwReaderLen));
            dwReaderLen = 127;
        }

        g_memset(reader_name, 0, sizeof(reader_name));
        in_uint8a(in_s, reader_name, dwReaderLen);

    }

    for (index = 0; index < 16; index++)
    {
        if (g_strcmp(reader_name, uds_client->readerStates[index].readerName) == 0)
        {
            LLOGLN(10, ("scard_function_status_return: found read [%s]", reader_name));
            LLOGLN(10, ("  updateing cardAtrLength from %d to %d", uds_client->readerStates[index].cardAtrLength, dwAtrLen));
            uds_client->readerStates[index].cardAtrLength = dwAtrLen;
            g_memcpy(uds_client->readerStates[index].cardAtr, attr, dwAtrLen);
            LLOGLN(10, ("  updateing cardProtocol from %d to %d", uds_client->readerStates[index].cardProtocol, dwProtocol));
            uds_client->readerStates[index].cardProtocol = dwProtocol;
            LLOGLN(10, ("  updateing dwState from %d to %d", uds_client->readerStates[index].readerState, dwState));
            uds_client->readerStates[index].readerState = dwState;
        }
    }

    con = uds_client->con;
    rv = 0;
    out_s = con->out_s;
    init_stream(out_s, 0);
    out_uint32_le(out_s, 0); /* hCard */
    out_uint32_le(out_s, return_code); /* rv */
    s_mark_end(out_s);
    rv = trans_write_copy(con);
    uds_client->ref_count--;
    free_uds_client(uds_client);
    return rv;
}

/*****************************************************************************/
/* returns error */
static int
scard_process_cmd_version(struct trans *con, struct stream *in_s)
{
    int rv;
    int major;
    int minor;
    struct pcsc_uds_client *uds_client;
    void *user_data;

    LLOGLN(10, ("scard_process_version:"));
    rv = 0;
    in_uint32_le(in_s, major);
    in_uint32_le(in_s, minor);
    LLOGLN(10, ("scard_process_version: major %d minor %d", major, minor));
    uds_client = (struct pcsc_uds_client *) (con->callback_data);
    uds_client->state = 1;
    uds_client = (struct pcsc_uds_client *) (con->callback_data);
    user_data = (void *) (tintptr) (uds_client->uds_client_id);
    uds_client->ref_count++;
    scard_send_establish_context(user_data, 0); /* SCARD_SCOPE_USER */
    return rv;
}

/*****************************************************************************/
/* returns error */
static int
scard_process_cmd_get_readers_state(struct trans *con, struct stream *in_s)
{
    int rv;
    struct stream *out_s;
    struct pcsc_uds_client *uds_client;

    LLOGLN(10, ("scard_process_cmd_get_readers_state:"));
    rv = 0;
    uds_client = (struct pcsc_uds_client *) (con->callback_data);
    out_s = con->out_s;
    init_stream(out_s, sizeof(uds_client->readerStates));
    out_uint8a(out_s, uds_client->readerStates, sizeof(uds_client->readerStates));
    s_mark_end(out_s);
    rv = trans_write_copy(con);
    return rv;
}

/*****************************************************************************/
/* returns error */
static int
scard_process_cmd_wait_reader_state_change(struct trans *con,
                                           struct stream *in_s)
{
    int rv;
    //struct stream *out_s;
    int timeOut;
    struct pcsc_uds_client *uds_client;

    LLOGLN(10, ("scard_process_cmd_wait_reader_state_change:"));
    in_uint32_le(in_s, timeOut);
    LLOGLN(10, ("scard_process_cmd_wait_reader_state_change: timeOut %d",
           timeOut));
    //out_s = con->out_s;
    uds_client = (struct pcsc_uds_client *) (con->callback_data);
    uds_client->waiting = 1;
    rv = 0;
    return rv;
}

/*****************************************************************************/
/* returns error */
static int
scard_process_cmd_stop_waiting_reader_state_change(struct trans *con,
                                                   struct stream *in_s)
{
    int rv;
    struct stream *out_s;
    struct pcsc_uds_client *uds_client;

    LLOGLN(10, ("scard_process_cmd_stop_waiting_reader_state_change:"));
    out_s = con->out_s;
    init_stream(out_s, 8192);
    out_uint32_le(out_s, 0); /* timeOut */
    out_uint32_le(out_s, 0); /* rv */
    s_mark_end(out_s);
    rv = trans_write_copy(con);
    uds_client = (struct pcsc_uds_client *) (con->callback_data);
    uds_client->waiting = 0;
    uds_client->something_changed = 0;
    return rv;
}

/*****************************************************************************/
/* returns error */
int
scard_process_get_status_change(struct trans *con, struct stream *in_s)
{
    return 0;
}

/*****************************************************************************/
int
scard_function_get_status_change_return(void *user_data,
                                        struct stream *in_s,
                                        int len, int status)
{
    int rv;
    int return_code;
    int cReaders;
    int index;
    int current_state;
    int event_state;
    int cardAtrLength;
    struct pcsc_uds_client *uds_client;
    struct stream *out_s;

    READER_STATE *rsa;
    struct pcsc_context *context;

    LLOGLN(10, ("scard_function_get_status_change_return:"));
    rv = 0;
    cReaders = 0;
    return_code = 0;
    uds_client = (struct pcsc_uds_client *) user_data;

    if (status == 0)
    {
        in_uint8s(in_s, 16);
        in_uint32_le(in_s, return_code);
        in_uint8s(in_s, 8);
        in_uint32_le(in_s, cReaders);
        LLOGLN(10, ("  cReaders %d", cReaders));
        if (return_code != 0)
        {
            cReaders = 0;
        }
        if (cReaders > 0)
        {
            for (index = 0; index < cReaders; index++)
            {
                in_uint32_le(in_s, current_state); /* current state */
                in_uint32_le(in_s, event_state); /* event state */
                LLOGLN(10, ("    index %d current_state 0x%8.8x event_state 0x%8.8x", index, current_state, event_state));
                uds_client->current_states[index] = current_state;
                uds_client->event_states[index] = event_state;
                if (event_state & 0x0002) /* SCARD_STATE_CHANGED */
                {
                    if (event_state & 0x0020) /* SCARD_STATE_PRESENT */
                    {
                        uds_client->readerStates[index].readerState = 0x0004; /* SCARD_PRESENT */
                    }
                    else
                    {
                        uds_client->readerStates[index].readerState = 0x0002; /* SCARD_ABSENT */
                    }
                    uds_client->something_changed = 1;
                    uds_client->send_status = 1;
                }
                in_uint32_le(in_s, uds_client->readerStates[index].cardAtrLength);
                in_uint8a(in_s, uds_client->readerStates[index].cardAtr, 36);
                uds_client->readerStates[index].eventCounter = (event_state >> 16) & 0xFFFF;
            }
        }

        context = get_first_pcsc_context(uds_client);

        if (uds_client->state == 1)
        {
            /* send version back */
            out_s = uds_client->con->out_s;
            init_stream(out_s, 8192);
            out_uint32_le(out_s, 4);
            out_uint32_le(out_s, 3);
            out_uint32_le(out_s, 0); /* result */
            s_mark_end(out_s);
            rv = trans_write_copy(uds_client->con);
            uds_client->state = 0;
            return_code = 0x8010000A;
        }

        if (return_code == 0x8010000A) /* SCARD_E_TIMEOUT */
        {
            rsa = g_new(READER_STATE, uds_client->numReaders + 1);
            for (index = 0; index < uds_client->numReaders; index++)
            {
                g_strncpy(rsa[index].reader_name, uds_client->readerStates[index].readerName, 127);
                rsa[index].current_state = uds_client->event_states[index] & ~2;
                rsa[index].event_state = uds_client->event_states[index];
                cardAtrLength = MIN(33, uds_client->readerStates[index].cardAtrLength);
                cardAtrLength = MAX(0, cardAtrLength);
                g_memcpy(rsa[index].atr, uds_client->readerStates[index].cardAtr, cardAtrLength);
                rsa[index].atr_len = cardAtrLength;
            }
            /* add plug and play notifier */
            g_strncpy(rsa[index].reader_name, "\\\\?PnP?\\Notification", 127);
            rsa[index].current_state = index << 16;
            uds_client->ref_count++;
            scard_send_get_status_change(uds_client,
                                         context->context,
                                         context->context_bytes,
                                         0, 0xFFFFFFFF, index + 1, rsa);
            g_free(rsa);

        }
        else if (return_code == 0)
        {
            uds_client->ref_count++;
            scard_send_list_readers(uds_client,
                                    context->context,
                                    context->context_bytes,
                                    0, 8148, 0, 0);
        }

    }

    LLOGLN(10, ("scard_function_get_status_change_return: something changed %d waiting %d cReaders %d return_code 0x%8.8x",
               uds_client->something_changed, uds_client->waiting, cReaders, return_code));

    if (cReaders == 0 && uds_client->waiting && uds_client->something_changed)
    {
        LLOGLN(10, ("scard_function_get_status_change_return: something changed"));
        uds_client->waiting = 0;
        uds_client->something_changed = 0;
        out_s = uds_client->con->out_s;
        init_stream(out_s, 8192);
        out_uint32_le(out_s, 0); /* timeOut */
        out_uint32_le(out_s, 0); /* rv */
        s_mark_end(out_s);
        rv = trans_write_copy(uds_client->con);
    }
    uds_client->ref_count--;
    free_uds_client(uds_client);
    return rv;
}

/*****************************************************************************/
/* returns error */
int
scard_process_cancel(struct trans *con, struct stream *in_s)
{
    int rv;
    struct stream *out_s;
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
        LLOGLN(0, ("scard_process_cancel: get_pcsc_context_by_app_context failed"));
        out_s = uds_client->con->out_s;
        init_stream(out_s, 8192);
        out_uint32_le(out_s, 0); /* hContext */
        out_uint32_le(out_s, 0); /* rv */
        s_mark_end(out_s);
        rv = trans_write_copy(uds_client->con);
        return rv;
    }
    uds_client->ref_count++;
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
    int rv;
    int uds_client_id;
    int return_code;
    struct stream *out_s;
    struct pcsc_uds_client *uds_client;

    LLOGLN(10, ("scard_function_cancel_return:"));
    LLOGLN(10, ("  status 0x%8.8x", status));
    uds_client_id = (int) (tintptr) user_data;
    uds_client = (struct pcsc_uds_client *)
                 get_uds_client_by_id(uds_client_id);
    if (uds_client == 0)
    {
        LLOGLN(0, ("scard_function_cancel_return: get_uds_client_by_id failed"));
        return 1;
    }
    return_code = 0;
    if (status == 0)
    {
        in_uint8s(in_s, 16); 
        in_uint32_le(in_s, return_code);
    }
    out_s = uds_client->con->out_s;
    init_stream(out_s, 8192);
    out_uint32_le(out_s, 0); /* hContext */
    out_uint32_le(out_s, return_code); /* rv */
    s_mark_end(out_s);
    rv = trans_write_copy(uds_client->con);
    uds_client->ref_count--;
    free_uds_client(uds_client);
    return rv;
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
            LLOGLN(10, ("scard_process_msg: SCARD_RECONNECT"));
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
            LLOGLN(10, ("scard_process_msg: SCARD_CANCEL_TRANSACTION"));
            break;

        case 0x0F: /* SCARD_GET_ATTRIB */
            LLOGLN(10, ("scard_process_msg: SCARD_GET_ATTRIB"));
            break;

        case 0x10: /* SCARD_SET_ATTRIB */
            LLOGLN(10, ("scard_process_msg: SCARD_SET_ATTRIB"));
            break;

        case 0x11: /* CMD_VERSION */
            LLOGLN(10, ("scard_process_msg: CMD_VERSION"));
            rv = scard_process_cmd_version(con, in_s);
            break;

        case 0x12: /* CMD_GET_READERS_STATE */
            LLOGLN(10, ("scard_process_msg: CMD_GET_READERS_STATE"));
            rv = scard_process_cmd_get_readers_state(con, in_s);
            break;

        case 0x13: /* CMD_WAIT_READER_STATE_CHANGE */
            LLOGLN(10, ("scard_process_msg: CMD_WAIT_READER_STATE_CHANGE"));
            rv = scard_process_cmd_wait_reader_state_change(con, in_s);
            break;

        case 0x14: /* CMD_STOP_WAITING_READER_STATE_CHANGE */
            LLOGLN(10, ("scard_process_msg: CMD_STOP_WAITING_READER_STATE_CHANGE"));
            rv = scard_process_cmd_stop_waiting_reader_state_change(con, in_s);
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
    error = 0;
    s = trans_get_in_s(trans);
    switch (trans->extra_flags)
    {
        case 0:
            s->p = s->data;
            in_uint32_le(s, size);
            in_uint32_le(s, command);
            LLOGLN(10, ("my_pcsc_trans_data_in: size %d command %d", size, command));
            trans->extra_flags = command; 
            if (size > 0)
            {
                trans->header_size = 8 + size;
                break;
            }
            /* fallthrough */
        default:
            LLOGLN(10, ("my_pcsc_trans_data_in: got payload"));
            s->p = s->data;
            in_uint8s(s, 4); /* size */
            in_uint32_le(s, command);
            LLOGLN(10, ("my_pcsc_trans_data_in: default command %d", command));
            error = scard_process_msg(trans, s, command);
            init_stream(s, 0);
            trans->header_size = 8;
            trans->extra_flags = 0;
            break;
        case 9: /* transmit */
            LLOGLN(10, ("my_pcsc_trans_data_in: special transmit"));
            s->p = s->data;
            in_uint8s(s, 12 + 8);
            in_uint32_le(s, size);
            s->p = s->data;
            if (size < 1)
            {
                in_uint8s(s, 4); /* size */
                in_uint32_le(s, command);
                LLOGLN(10, ("my_pcsc_trans_data_in: 9 command %d", command));
                error = scard_process_msg(trans, s, command);
                init_stream(s, 0);
                trans->header_size = 8;
                trans->extra_flags = 0;
                break;
            }
            trans->header_size += size;
            trans->extra_flags = 999;
            break;
        case 999: /* transmit */
            LLOGLN(10, ("my_pcsc_trans_data_in: special transmit 999"));
            s->p = s->data;
            in_uint8s(s, 4); /* size */
            in_uint32_le(s, command);
            LLOGLN(10, ("my_pcsc_trans_data_in: 999 command %d", command));
            error = scard_process_msg(trans, s, command);
            init_stream(s, 0);
            trans->header_size = 8;
            trans->extra_flags = 0;
            break;
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

    LLOGLN(0, ("my_pcsc_trans_conn_in: got connection"));
    uds_client = create_uds_client(new_trans);
    if (uds_client == 0)
    {
        return 1;
    }
    uds_client->con->trans_data_in = my_pcsc_trans_data_in;
    uds_client->con->header_size = 8;
    uds_client->con->no_stream_init_on_data_in = 1;

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
    const char *csock_name;
    int disp;
    int error;

    LLOGLN(0, ("scard_pcsc_init:"));
    if (g_lis == 0)
    {
        g_lis = trans_create(2, 8192, 8192);
        csock_name = g_getenv("XRDP_PCSCLITE_CSOCK_NAME");
        if (csock_name != 0)
        {
            g_snprintf(g_pcsclite_ipc_file, 255, "%s", csock_name);
        }
        else
        {
            disp = g_display_num;
            g_snprintf(g_pcsclite_ipc_file, 255, "/tmp/.xrdp/pcscd%d.com", disp);
        }
        LLOGLN(0, ("scard_pcsc_init: trans_listen on port %s", g_pcsclite_ipc_file));
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
    int index;
    struct pcsc_uds_client *uds_client;

    LLOGLN(0, ("scard_pcsc_deinit:"));

    if (g_uds_clients != 0)
    {
        for (index = g_uds_clients->count - 1; index >= 0; index--)
        {
            uds_client = (struct pcsc_uds_client *)
                         list_get_item(g_uds_clients, index);
            uds_client->ref_count = 0;
            free_uds_client(uds_client);
            list_remove_item(g_uds_clients, index);
        }
    }

    if (g_lis != 0)
    {
        trans_delete(g_lis);
        g_lis = 0;
    }

    if (g_pcsclite_ipc_file[0] != 0)
    {
        g_file_delete(g_pcsclite_ipc_file);
        g_pcsclite_ipc_file[0] = 0;
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
