/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Laxmikant Rashinkar 2013 LK.Rashinkar@gmail.com
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

/**
 * @file sesman/chansrv/smartcard.c
 *
 * smartcard redirection support
 *
 * This file implements some of the PDUs detailed in [MS-RDPESC].
 *
 * The PDUs use DCE IDL structs. These are required to be re-interpreted
 * in DCE NDR (Netword Data Representation)
 *
 * For more information on this subject see DCE publication C706
 * "DCE 1.1: Remote Procedure Call" 1997. In particular:-
 * Section 4.2 : Describes the IDL
 * Section 14 : Describes the NDR
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <string.h>
#include "os_calls.h"
#include "string_calls.h"
#include "smartcard.h"
#include "log.h"
#include "irp.h"
#include "devredir.h"
#include "smartcard_pcsc.h"
#include "chansrv.h"

/*
 * TODO
 *
 * o ensure that all wide calls are handled correctly
 *
 * o need to query client for build number and determine whether we should use
 *   SCREDIR_VERSION_XP or SCREDIR_VERSION_LONGHORN
 *
 * o need to call scard_release_resources()
 *
 * o why is win 7 sending SCARD_IOCTL_ACCESS_STARTED_EVENT first
 * 0000 00 01 00 00 04 00 00 00 e0 00 09 00 00 00 00 00 ................
 * 0010 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
 * 0020 28 b7 9d 02
 */

/*
 * Notes:
 *
 * XP and Server 2003 use version    SCREDIR_VERSION_XP       functions 5 - 58
 * Vista and Server 2008 use version SCREDIR_VERSION_LONGHORN functions 5 - 64
 * if TS Client's build number is >= 4,034 use SCREDIR_VERSION_LONGHORN
 */

/* [MS-RDPESC] 3.1.4 */
#define SCARD_IOCTL_ESTABLISH_CONTEXT        0x00090014 /* EstablishContext     */
#define SCARD_IOCTL_RELEASE_CONTEXT          0x00090018 /* ReleaseContext       */
#define SCARD_IOCTL_IS_VALID_CONTEXT         0x0009001C /* IsValidContext       */
#define SCARD_IOCTL_LIST_READER_GROUPS       0x00090020 /* ListReaderGroups     */
#define SCARD_IOCTL_LIST_READERS_A           0x00090028 /* ListReaders ASCII    */
#define SCARD_IOCTL_LIST_READERS_W           0x0009002C /* ListReaders Wide     */
#define SCARD_IOCTL_INTRODUCE_READER_GROUP   0x00090050 /* IntroduceReaderGroup */
#define SCARD_IOCTL_FORGET_READER_GROUP      0x00090058 /* ForgetReader         */
#define SCARD_IOCTL_INTRODUCE_READER         0x00090060 /* IntroduceReader      */
#define SCARD_IOCTL_FORGET_READER            0x00090068 /* IntroduceReader      */
#define SCARD_IOCTL_ADD_READER_TO_GROUP      0x00090070 /* AddReaderToGroup     */
#define SCARD_IOCTL_REMOVE_READER_FROM_GROUP 0x00090078 /* RemoveReaderFromGroup*/
#define SCARD_IOCTL_GET_STATUS_CHANGE_A      0x000900A0 /* GetStatusChangeA     */
#define SCARD_IOCTL_GET_STATUS_CHANGE_W      0x000900A4 /* GetStatusChangeW     */
#define SCARD_IOCTL_CANCEL                   0x000900A8 /* Cancel               */
#define SCARD_IOCTL_CONNECT_A                0x000900AC /* ConnectA             */
#define SCARD_IOCTL_CONNECT_W                0x000900B0 /* ConnectW             */
#define SCARD_IOCTL_RECONNECT                0x000900B4 /* Reconnect            */
#define SCARD_IOCTL_DISCONNECT               0x000900B8 /* Disconnect           */
#define SCARD_IOCTL_BEGIN_TRANSACTION        0x000900BC /* BeginTransaction     */
#define SCARD_IOCTL_END_TRANSACTION          0x000900C0 /* EndTransaction       */
#define SCARD_IOCTL_STATE                    0x000900C4 /* State                */
#define SCARD_IOCTL_STATUS_A                 0x000900C8 /* StatusA              */
#define SCARD_IOCTL_STATUS_W                 0x000900CC /* StatusW              */
#define SCARD_IOCTL_TRANSMIT                 0x000900D0 /* Transmit             */
#define SCARD_IOCTL_CONTROL                  0x000900D4 /* Control              */
#define SCARD_IOCTL_GETATTRIB                0x000900D8 /* GetAttrib            */
#define SCARD_IOCTL_SETATTRIB                0x000900DC /* SetAttrib            */
#define SCARD_IOCTL_ACCESS_STARTED_EVENT     0x000900E0 /* SCardAccessStartedEvent */
#define SCARD_IOCTL_LOCATE_CARDS_BY_ATR      0x000900E8 /* LocateCardsByATR     */

/* scope used in EstablishContextCall */
#define SCARD_SCOPE_USER                     0x00000000
#define SCARD_SCOPE_TERMINAL                 0x00000001
#define SCARD_SCOPE_SYSTEM                   0x00000002

/* disposition - action to take on card */
#define SCARD_LEAVE_CARD                     0x00000000
#define SCARD_RESET_CARD                     0x00000001
#define SCARD_UNPOWER_CARD                   0x00000002
#define SCARD_EJECT_CARD                     0x00000003

#define MAX_SMARTCARDS                       16

/* stores info about a smart card */
typedef struct smartcard
{
    tui32 DeviceId;
} SMARTCARD;

/* globals */
SMARTCARD   *smartcards[MAX_SMARTCARDS];
int          g_smartcards_inited = 0;
static tui32 g_device_id = 0;
static int   g_scard_index = 0;

/* externs */
extern tui32 g_completion_id;
extern int   g_rdpdr_chan_id;    /* in chansrv.c */


/******************************************************************************
**                   static functions local to this file                     **
******************************************************************************/
static struct stream *scard_make_new_ioctl(IRP *irp, tui32 ioctl);
static int  scard_add_new_device(tui32 device_id);
static int  scard_get_free_slot(void);
static void scard_release_resources(void);
static void scard_send_EstablishContext(IRP *irp, int scope);
static void scard_send_ReleaseContext(IRP *irp,
                                      char *context, int context_bytes);
static void scard_send_IsContextValid(IRP *irp,
                                      char *context, int context_bytes);
static void scard_send_ListReaders(IRP *irp,
                                   char *context, int context_bytes,
                                   char *groups, int cchReaders,
                                   int wide);
static void scard_send_GetStatusChange(IRP *irp,
                                       char *context, int context_bytes,
                                       int wide,
                                       tui32 timeout, tui32 num_readers,
                                       READER_STATE *rsa);
static void scard_send_Connect(IRP *irp,
                               char *context, int context_bytes,
                               int wide,
                               READER_STATE *rs);
static void scard_send_Reconnect(IRP *irp,
                                 char *context, int context_bytes,
                                 char *card, int card_bytes,
                                 READER_STATE *rs);
static void scard_send_BeginTransaction(IRP *irp,
                                        char *context, int context_bytes,
                                        char *card, int card_bytes);
static void scard_send_EndTransaction(IRP *irp,
                                      char *context, int context_bytes,
                                      char *card, int card_bytes,
                                      tui32 dwDisposition);
static void scard_send_Status(IRP *irp, int wide,
                              char *context, int context_bytes,
                              char *card, int card_bytes,
                              int cchReaderLen, int cbAtrLen);
static void scard_send_Disconnect(IRP *irp,
                                  char *context, int context_bytes,
                                  char *card, int card_bytes,
                                  int dwDisposition);
static int  scard_send_Transmit(IRP *irp,
                                char *context, int context_byte,
                                char *card, int card_bytes,
                                char *send_data, int send_bytes,
                                int recv_bytes,
                                struct xrdp_scard_io_request *send_ior,
                                struct xrdp_scard_io_request *recv_ior);
static int scard_send_Control(IRP *irp, char *context, int context_bytes,
                              char *card, int card_bytes,
                              char *send_data, int send_bytes,
                              int recv_bytes, int control_code);
static int scard_send_Cancel(IRP *irp, char *context, int context_bytes);
static int scard_send_GetAttrib(IRP *irp, char *card, int card_bytes,
                                READER_STATE *rs);

/******************************************************************************
**                    local callbacks into this module                       **
******************************************************************************/

static void scard_handle_EstablishContext_Return(struct stream *s, IRP *irp,
        tui32 DeviceId, tui32 CompletionId,
        tui32 IoStatus);

static void scard_handle_ReleaseContext_Return(struct stream *s, IRP *irp,
        tui32 DeviceId, tui32 CompletionId,
        tui32 IoStatus);


static void scard_handle_IsContextValid_Return(struct stream *s, IRP *irp,
        tui32 DeviceId, tui32 CompletionId,
        tui32 IoStatus);

static void scard_handle_ListReaders_Return(struct stream *s, IRP *irp,
        tui32 DeviceId, tui32 CompletionId,
        tui32 IoStatus);

static void scard_handle_GetStatusChange_Return(struct stream *s, IRP *irp,
        tui32 DeviceId, tui32 CompletionId,
        tui32 IoStatus);

static void scard_handle_Connect_Return(struct stream *s, IRP *irp,
                                        tui32 DeviceId, tui32 CompletionId,
                                        tui32 IoStatus);

static void scard_handle_Reconnect_Return(struct stream *s, IRP *irp,
        tui32 DeviceId, tui32 CompletionId,
        tui32 IoStatus);

static void scard_handle_BeginTransaction_Return(struct stream *s, IRP *irp,
        tui32 DeviceId, tui32 CompletionId,
        tui32 IoStatus);

static void scard_handle_EndTransaction_Return(struct stream *s, IRP *irp,
        tui32 DeviceId,
        tui32 CompletionId,
        tui32 IoStatus);

static void scard_handle_Status_Return(struct stream *s, IRP *irp,
                                       tui32 DeviceId, tui32 CompletionId,
                                       tui32 IoStatus);

static void scard_handle_Disconnect_Return(struct stream *s, IRP *irp,
        tui32 DeviceId, tui32 CompletionId,
        tui32 IoStatus);


static void scard_handle_Transmit_Return(struct stream *s, IRP *irp,
        tui32 DeviceId,
        tui32 CompletionId,
        tui32 IoStatus);

static void scard_handle_Control_Return(struct stream *s, IRP *irp,
                                        tui32 DeviceId,
                                        tui32 CompletionId,
                                        tui32 IoStatus);

static void scard_handle_Cancel_Return(struct stream *s, IRP *irp,
                                       tui32 DeviceId,
                                       tui32 CompletionId,
                                       tui32 IoStatus);

static void scard_handle_GetAttrib_Return(struct stream *s, IRP *irp,
        tui32 DeviceId,
        tui32 CompletionId,
        tui32 IoStatus);

/******************************************************************************
**                                                                           **
**          externally accessible functions, defined in smartcard.h          **
**                                                                           **
******************************************************************************/

/**
 *****************************************************************************/
void
scard_device_announce(tui32 device_id)
{
    LOG_DEVEL(LOG_LEVEL_DEBUG, "entered: device_id=%d", device_id);

    if (g_smartcards_inited)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "already init");
        return;
    }

    g_memset(&smartcards, 0, sizeof(smartcards));
    g_smartcards_inited = 1;
    g_device_id = device_id;
    g_scard_index = scard_add_new_device(device_id);

    if (g_scard_index < 0)
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "scard_add_new_device failed with DeviceId=%d", g_device_id);
    }
    else
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "added smartcard with DeviceId=%d to list", g_device_id);
    }
}

/**
 *
 *****************************************************************************/
int
scard_get_wait_objs(tbus *objs, int *count, int *timeout)
{
    return scard_pcsc_get_wait_objs(objs, count, timeout);
}

/**
 *
 *****************************************************************************/
int
scard_check_wait_objs(void)
{
    return scard_pcsc_check_wait_objs();
}

/**
 *
 *****************************************************************************/
int
scard_init(void)
{
    LOG_DEVEL(LOG_LEVEL_INFO, "scard_init:");
    return scard_pcsc_init();
}

/**
 *
 *****************************************************************************/
int
scard_deinit(void)
{
    LOG_DEVEL(LOG_LEVEL_INFO, "scard_deinit:");
    scard_pcsc_deinit();
    scard_release_resources();
    g_smartcards_inited = 0;
    return 0;
}

/**
 *
 *****************************************************************************/
int
scard_send_establish_context(void *user_data, int scope)
{
    IRP *irp;

    /* setup up IRP */
    if ((irp = devredir_irp_new()) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "system out of memory");
        return 1;
    }

    irp->scard_index = g_scard_index;
    irp->CompletionId = g_completion_id++;
    irp->DeviceId = g_device_id;
    irp->callback = scard_handle_EstablishContext_Return;
    irp->user_data = user_data;

    /* send IRP to client */
    scard_send_EstablishContext(irp, scope);

    return 0;
}

/**
 * Release a previously established Smart Card context
 *****************************************************************************/
int
scard_send_release_context(void *user_data,
                           char *context, int context_bytes)
{
    IRP *irp;

    /* setup up IRP */
    if ((irp = devredir_irp_new()) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "system out of memory");
        return 1;
    }

    irp->scard_index = g_scard_index;
    irp->CompletionId = g_completion_id++;
    irp->DeviceId = g_device_id;
    irp->callback = scard_handle_ReleaseContext_Return;
    irp->user_data = user_data;

    /* send IRP to client */
    scard_send_ReleaseContext(irp, context, context_bytes);

    return 0;
}

/**
 * Checks if a previously established context is still valid
 *****************************************************************************/
int
scard_send_is_valid_context(void *user_data, char *context, int context_bytes)
{
    IRP *irp;

    /* setup up IRP */
    if ((irp = devredir_irp_new()) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "system out of memory");
        return 1;
    }

    irp->scard_index = g_scard_index;
    irp->CompletionId = g_completion_id++;
    irp->DeviceId = g_device_id;
    irp->callback = scard_handle_IsContextValid_Return;
    irp->user_data = user_data;

    /* send IRP to client */
    scard_send_IsContextValid(irp, context, context_bytes);

    return 0;
}

/**
 *
 *****************************************************************************/
int
scard_send_list_readers(void *user_data, char *context, int context_bytes,
                        char *groups, int cchReaders, int wide)
{
    IRP *irp;

    /* setup up IRP */
    if ((irp = devredir_irp_new()) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "system out of memory");
        return 1;
    }
    irp->scard_index = g_scard_index;
    irp->CompletionId = g_completion_id++;
    irp->DeviceId = g_device_id;
    irp->callback = scard_handle_ListReaders_Return;
    irp->user_data = user_data;

    /* send IRP to client */
    scard_send_ListReaders(irp, context, context_bytes, groups,
                           cchReaders, wide);

    return 0;
}

/**
 * Send get change in status command
 *
 * @param  con          connection to client
 * @param  wide         TRUE if unicode string
 * @param  timeout      timeout in milliseconds, -1 for infinity
 * @param  num_readers  number of entries in rsa
 * @param  rsa          array of READER_STATEs
 *****************************************************************************/
int
scard_send_get_status_change(void *user_data, char *context, int context_bytes,
                             int wide, tui32 timeout, tui32 num_readers,
                             READER_STATE *rsa)
{
    IRP *irp;

    /* setup up IRP */
    if ((irp = devredir_irp_new()) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "system out of memory");
        return 1;
    }

    irp->scard_index = g_scard_index;
    irp->CompletionId = g_completion_id++;
    irp->DeviceId = g_device_id;
    irp->callback = scard_handle_GetStatusChange_Return;
    irp->user_data = user_data;

    /* send IRP to client */
    scard_send_GetStatusChange(irp, context, context_bytes, wide, timeout,
                               num_readers, rsa);

    return 0;
}

/**
 * Open a connection to the smart card located in the reader
 *
 * @param  con   connection to client
 * @param  wide  TRUE if unicode string
 *****************************************************************************/
int
scard_send_connect(void *user_data, char *context, int context_bytes,
                   int wide, READER_STATE *rs)
{
    IRP *irp;

    /* setup up IRP */
    if ((irp = devredir_irp_new()) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "system out of memory");
        return 1;
    }

    irp->scard_index = g_scard_index;
    irp->CompletionId = g_completion_id++;
    irp->DeviceId = g_device_id;
    irp->callback = scard_handle_Connect_Return;
    irp->user_data = user_data;

    /* send IRP to client */
    scard_send_Connect(irp, context, context_bytes, wide, rs);

    return 0;
}

/**
 * The reconnect method re-establishes a smart card reader handle. On success,
 * the handle is valid once again.
 *
 * @param  con        connection to client
 * @param  sc_handle  handle to device
 * @param  rs         reader state where following fields are set
 *                        rs.shared_mode_flag
 *                        rs.preferred_protocol
 *                        rs.init_type
 *****************************************************************************/
int
scard_send_reconnect(void *user_data, char *context, int context_bytes,
                     char *card, int card_bytes, READER_STATE *rs)
{
    IRP *irp;

    /* setup up IRP */
    if ((irp = devredir_irp_new()) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "system out of memory");
        return 1;
    }

    irp->scard_index = g_scard_index;
    irp->CompletionId = g_completion_id++;
    irp->DeviceId = g_device_id;
    irp->callback = scard_handle_Reconnect_Return;
    irp->user_data = user_data;

    /* send IRP to client */
    scard_send_Reconnect(irp, context, context_bytes, card, card_bytes, rs);

    return 0;
}

/**
 * Lock smart card reader for exclusive access for specified smart
 * card reader handle.
 *
 * @param  con   connection to client
 *****************************************************************************/
int
scard_send_begin_transaction(void *user_data, char *context, int context_bytes,
                             char *card, int card_bytes)
{
    IRP *irp;

    /* setup up IRP */
    if ((irp = devredir_irp_new()) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "system out of memory");
        return 1;
    }

    irp->scard_index = g_scard_index;
    irp->CompletionId = g_completion_id++;
    irp->DeviceId = g_device_id;
    irp->callback = scard_handle_BeginTransaction_Return;
    irp->user_data = user_data;

    /* send IRP to client */
    scard_send_BeginTransaction(irp, context, context_bytes, card, card_bytes);

    return 0;
}

/**
 * Release a smart card reader after being locked by a previously
 * successful call to Begin Transaction
 *
 * @param  con        connection to client
 * @param  sc_handle  handle to smartcard
 *****************************************************************************/
int
scard_send_end_transaction(void *user_data, char *context, int context_bytes,
                           char *card, int card_bytes,
                           tui32 dwDisposition)
{
    IRP *irp;

    /* setup up IRP */
    if ((irp = devredir_irp_new()) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "system out of memory");
        return 1;
    }

    irp->scard_index = g_scard_index;
    irp->CompletionId = g_completion_id++;
    irp->DeviceId = g_device_id;
    irp->callback = scard_handle_EndTransaction_Return;
    irp->user_data = user_data;

    /* send IRP to client */
    scard_send_EndTransaction(irp, context, context_bytes,
                              card, card_bytes, dwDisposition);

    return 0;
}

/**
 * Get the status of a connection for a valid smart card reader handle
 *
 * @param  con   connection to client
 * @param  wide  TRUE if unicode string
 *****************************************************************************/
int
scard_send_status(void *user_data, int wide, char *context, int context_bytes,
                  char *card, int card_bytes,
                  int cchReaderLen, int cbAtrLen)
{
    IRP *irp;

    /* setup up IRP */
    if ((irp = devredir_irp_new()) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "system out of memory");
        return 1;
    }

    irp->scard_index = g_scard_index;
    irp->CompletionId = g_completion_id++;
    irp->DeviceId = g_device_id;
    irp->callback = scard_handle_Status_Return;
    irp->user_data = user_data;

    /* send IRP to client */
    scard_send_Status(irp, wide, context, context_bytes, card, card_bytes,
                      cchReaderLen, cbAtrLen);

    return 0;
}

/**
 * Release a smart card reader handle that was acquired in ConnectA/ConnectW
 *
 * @param  con        connection to client
 * @param  sc_handle  handle to smartcard
 *****************************************************************************/
int
scard_send_disconnect(void *user_data, char *context, int context_bytes,
                      char *card, int card_bytes, int dwDisposition)
{
    IRP *irp;

    /* setup up IRP */
    if ((irp = devredir_irp_new()) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "system out of memory");
        return 1;
    }

    irp->scard_index = g_scard_index;
    irp->CompletionId = g_completion_id++;
    irp->DeviceId = g_device_id;
    irp->callback = scard_handle_Disconnect_Return;
    irp->user_data = user_data;

    /* send IRP to client */
    scard_send_Disconnect(irp, context, context_bytes,
                          card, card_bytes, dwDisposition);

    return 0;
}

/**
 * The Transmit_Call structure is used to send data to the smart card
 * associated with a valid context.
 *****************************************************************************/
int
scard_send_transmit(void *user_data, char *context, int context_bytes,
                    char *card, int card_bytes,
                    char *send_data, int send_bytes, int recv_bytes,
                    struct xrdp_scard_io_request *send_ior,
                    struct xrdp_scard_io_request *recv_ior)
{
    IRP *irp;

    /* setup up IRP */
    if ((irp = devredir_irp_new()) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "system out of memory");
        return 1;
    }

    irp->scard_index = g_scard_index;
    irp->CompletionId = g_completion_id++;
    irp->DeviceId = g_device_id;
    irp->callback = scard_handle_Transmit_Return;
    irp->user_data = user_data;

    /* send IRP to client */
    scard_send_Transmit(irp, context, context_bytes, card, card_bytes,
                        send_data, send_bytes,
                        recv_bytes, send_ior, recv_ior);

    return 0;
}

/**
 * Communicate directly with the smart card reader
 *****************************************************************************/
int
scard_send_control(void *user_data, char *context, int context_bytes,
                   char *card, int card_bytes,
                   char *send_data, int send_bytes,
                   int recv_bytes, int control_code)
{
    IRP *irp;

    /* setup up IRP */
    if ((irp = devredir_irp_new()) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "system out of memory");
        return 1;
    }

    irp->scard_index = g_scard_index;
    irp->CompletionId = g_completion_id++;
    irp->DeviceId = g_device_id;
    irp->callback = scard_handle_Control_Return;
    irp->user_data = user_data;

    /* send IRP to client */
    scard_send_Control(irp, context, context_bytes,
                       card, card_bytes,
                       send_data, send_bytes,
                       recv_bytes, control_code);

    return 0;
}

/**
 * Cancel any outstanding calls
 *****************************************************************************/
int
scard_send_cancel(void *user_data, char *context, int context_bytes)
{
    IRP *irp;

    /* setup up IRP */
    if ((irp = devredir_irp_new()) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "system out of memory");
        return 1;
    }

    irp->scard_index = g_scard_index;
    irp->CompletionId = g_completion_id++;
    irp->DeviceId = g_device_id;
    irp->callback = scard_handle_Cancel_Return;
    irp->user_data = user_data;

    /* send IRP to client */
    scard_send_Cancel(irp, context, context_bytes);

    return 0;
}

/**
 * Get reader attributes
 *****************************************************************************/
int
scard_send_get_attrib(void *user_data, char *card, int card_bytes,
                      READER_STATE *rs)
{
    IRP *irp;

    /* setup up IRP */
    if ((irp = devredir_irp_new()) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "system out of memory");
        return 1;
    }

    irp->scard_index = g_scard_index;
    irp->CompletionId = g_completion_id++;
    irp->DeviceId = g_device_id;
    irp->callback = scard_handle_GetAttrib_Return;
    irp->user_data = user_data;

    /* send IRP to client */
    scard_send_GetAttrib(irp, card, card_bytes, rs);

    return 0;
}

/******************************************************************************
**                                                                           **
**                   static functions local to this file                     **
**                                                                           **
******************************************************************************/

/**
 * Create a new stream and insert specified IOCTL
 *
 * @param  irp    information about the I/O
 * @param  ioctl  the IOCTL code
 *
 * @return stream with IOCTL inserted in it, NULL on error
 *****************************************************************************/
static struct stream *
scard_make_new_ioctl(IRP *irp, tui32 ioctl)
{
    /*
     * format of device control request
     *
     * DeviceIoRequest
     * u16       RDPDR_CTYP_CORE
     * u16       PAKID_CORE_DEVICE_IOREQUEST
     * u32       DeviceId
     * u32       FileId
     * u32       CompletionId
     * u32       MajorFunction
     * u32       MinorFunction
     *
     * u32       OutputBufferLength SHOULD be 2048
     * u32       InputBufferLength
     * u32       IoControlCode
     * 20 bytes  padding
     * xx bytes  InputBuffer (variable)
     */

    struct stream *s;

    xstream_new(s, 1024 * 4);

    devredir_insert_DeviceIoRequest(s,
                                    irp->DeviceId,
                                    irp->FileId,
                                    irp->CompletionId,
                                    IRP_MJ_DEVICE_CONTROL,
                                    IRP_MN_NONE);

    xstream_wr_u32_le(s, 2048);        /* OutputBufferLength               */
    s_push_layer(s, iso_hdr, 4);       /* InputBufferLength - insert later */
    xstream_wr_u32_le(s, ioctl);       /* Ioctl Code                       */
    out_uint8s(s, 20);                 /* padding                          */

    /* [MS-RPCE] 2.2.6.1 */
    xstream_wr_u32_le(s, 0x00081001);  /* len 8, LE, v1                    */
    xstream_wr_u32_le(s, 0xcccccccc);  /* filler                           */

    return s;
}

/**
 * Create a new smart card device entry and insert it into smartcards[]
 *
 * @param  device_id  DeviceId of new card
 *
 * @return index into smartcards[] on success, -1 on failure
 *****************************************************************************/
static int
scard_add_new_device(tui32 device_id)
{
    int        index;
    SMARTCARD *sc;

    if ((index = scard_get_free_slot()) < 0)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "scard_get_free_slot failed");
        return -1;
    }

    sc = g_new0(SMARTCARD, 1);
    if (sc == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "system out of memory");
        return -1;
    }

    sc->DeviceId = device_id;
    smartcards[index] = sc;

    return index;
}

/**
 * Find first unused entry in smartcards
 *
 * @return index of first unused entry in smartcards or -1 if smartcards
 * is full
 *****************************************************************************/
static int
scard_get_free_slot(void)
{
    int i;

    for (i = 0; i < MAX_SMARTCARDS; i++)
    {
        if (smartcards[i] == NULL)
        {
            LOG_DEVEL(LOG_LEVEL_DEBUG, "found free slot at index %d", i);
            return i;
        }
    }

    LOG_DEVEL(LOG_LEVEL_ERROR, "too many smart card devices; rejecting this one");
    return -1;
}

/**
 * Release resources prior to shutting down
 *****************************************************************************/
static void
scard_release_resources(void)
{
    int i;

    for (i = 0; i < MAX_SMARTCARDS; i++)
    {
        if (smartcards[i] != NULL)
        {
            g_free(smartcards[i]);
            smartcards[i] = NULL;
        }
    }
}

/**
 *
 *****************************************************************************/
static void
scard_send_EstablishContext(IRP *irp, int scope)
{
    struct stream *s;
    int            bytes;

    if ((s = scard_make_new_ioctl(irp, SCARD_IOCTL_ESTABLISH_CONTEXT)) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "scard_make_new_ioctl failed");
        return;
    }

    s_push_layer(s, mcs_hdr, 4); /* bytes, set later */
    out_uint32_le(s, 0x00000000);
    out_uint32_le(s, scope);
    out_uint32_le(s, 0x00000000);

    s_mark_end(s);

    s_pop_layer(s, mcs_hdr);
    bytes = (int) (s->end - s->p);
    bytes -= 8;
    out_uint32_le(s, bytes);

    s_pop_layer(s, iso_hdr);
    bytes = (int) (s->end - s->p);
    bytes -= 28;
    out_uint32_le(s, bytes);

    bytes = (int) (s->end - s->data);

    /* send to client */
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);

    free_stream(s);
}

/**
 * Release a previously established Smart Card context
 *****************************************************************************/
static void
scard_send_ReleaseContext(IRP *irp, char *context, int context_bytes)
{
    /* see [MS-RDPESC] 3.1.4.2 */

    SMARTCARD     *sc;
    struct stream *s;
    int            bytes;

    if ((sc = smartcards[irp->scard_index]) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "smartcards[%d] is NULL", irp->scard_index);
        return;
    }

    if ((s = scard_make_new_ioctl(irp, SCARD_IOCTL_RELEASE_CONTEXT)) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "scard_make_new_ioctl failed");
        return;
    }

    s_push_layer(s, mcs_hdr, 4); /* bytes, set later */
    out_uint32_le(s, 0x00000000);
    out_uint32_le(s, context_bytes);
    out_uint32_le(s, 0x00020000);
    out_uint32_le(s, context_bytes);
    out_uint8a(s, context, context_bytes);

    s_mark_end(s);

    s_pop_layer(s, mcs_hdr);
    bytes = (int) (s->end - s->p);
    bytes -= 8;
    out_uint32_le(s, bytes);

    s_pop_layer(s, iso_hdr);
    bytes = (int) (s->end - s->p);
    bytes -= 28;
    out_uint32_le(s, bytes);

    bytes = (int) (s->end - s->data);

    /* send to client */
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);

    free_stream(s);
}

/**
 * Checks if a previously established context is still valid
 *****************************************************************************/
static void
scard_send_IsContextValid(IRP *irp, char *context, int context_bytes)
{
    /* see [MS-RDPESC] 3.1.4.3 */

    SMARTCARD     *sc;
    struct stream *s;
    int            bytes;

    if ((sc = smartcards[irp->scard_index]) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "smartcards[%d] is NULL", irp->scard_index);
        return;
    }

    if ((s = scard_make_new_ioctl(irp, SCARD_IOCTL_IS_VALID_CONTEXT)) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "scard_make_new_ioctl failed");
        return;
    }

    /*
     * command format
     *
     * ......
     *       20 bytes    padding
     * u32    4 bytes    len 8, LE, v1
     * u32    4 bytes    filler
     *       16 bytes    unused (s->p currently pointed here at unused[0])
     * u32    4 bytes    context len
     * u32    4 bytes    context
     */

    s_push_layer(s, mcs_hdr, 4); /* bytes, set later */

    /* insert context */
    out_uint32_le(s, context_bytes);
    out_uint8a(s, context, context_bytes);

    s_mark_end(s);

    s_pop_layer(s, mcs_hdr);
    bytes = (int) (s->end - s->p);
    bytes -= 8;
    out_uint32_le(s, bytes);

    s_pop_layer(s, iso_hdr);
    bytes = (int) (s->end - s->p);
    bytes -= 28;
    out_uint32_le(s, bytes);

    bytes = (int) (s->end - s->data);

    /* send to client */
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);

    free_stream(s);
}

/*****************************************************************************/
static void
align_s(struct stream *s, unsigned int boundary)
{
    unsigned int over = (unsigned int)(s->p - s->data) % boundary;
    if (over != 0)
    {
        out_uint8s(s, boundary - over);
    }
}

/**
 *
 *****************************************************************************/
static void
scard_send_ListReaders(IRP *irp, char *context, int context_bytes,
                       char *groups, int cchReaders, int wide)
{
    /* see [MS-RDPESC] 2.2.2.4
     *
     * IDL:-
     *
     * typedef struct _REDIR_SCARDCONTEXT {
     *    [range(0,16)] unsigned long cbContext;
     *    [unique] [size_is(cbContext)] byte *pbContext;
     *    } REDIR_SCARDCONTEXT;
     *
     * struct _ListReaders_Call {
     *     REDIR_SCARDCONTEXT Context;
     *     [range(0, 65536)] unsigned long cBytes;
     *     [unique] [size_is(cBytes)] const byte *mszGroups;
     *     long fmszReadersIsNULL;
     *     unsigned long cchReaders;
     *     } ListReaders_Call;
     *
     * Type summary:-
     *
     * Context.cbContext  Unsigned 32-bit word
     * Context.pbContext  Embedded full pointer to conformant array of bytes
     * cBytes             Unsigned 32-bit word
     * mszGroups          Embedded full pointer to conformant array of bytes
     * fmszReaders        32-bit word
     * cchReaders         Unsigned 32-bit word
     *
     * NDL:-
     *
     * Offset   Decription
     * 0        Context.cbContext
     * 4        Referent Identifier for pbContext
     * 8        cBytes
     * 12       Referent Identifier for mszGroups (or NULL)
     * 16       fmszReadersIsNULL
     * 20       cchReaders
     * 24       Conformant Array pointed to by pbContext
     * ??       Conformant Array pointed to by mszGroups
     *
     */

    SMARTCARD     *sc;
    struct stream *s;
    int            bytes;
    int            bytes_groups = 0; // Length of NDR for groups + 2 terminators
    int            val = 0;    // Referent Id for mszGroups (assume NULL)
    int            groups_len = 0; // strlen(groups)
    tui32          ioctl;

    if ((sc = smartcards[irp->scard_index]) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "smartcards[%d] is NULL", irp->scard_index);
        return;
    }

    ioctl = (wide) ? SCARD_IOCTL_LIST_READERS_W :
            SCARD_IOCTL_LIST_READERS_A;

    if ((s = scard_make_new_ioctl(irp, ioctl)) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "scard_make_new_ioctl failed");
        return;
    }

    if (groups != NULL && *groups != '\0')
    {
        groups_len = g_strlen(groups);
        if (wide)
        {
            bytes_groups = (utf8_as_utf16_word_count(groups, groups_len) + 2) * 2;
        }
        else
        {
            bytes_groups = groups_len + 2;
        }
        val = 0x00020004;
    }

    s_push_layer(s, mcs_hdr, 4); /* bytes, set later */
    out_uint32_le(s, 0x00000000);
    // REDIR_SCARDCONTEXT Context;
    out_uint32_le(s, context_bytes);
    out_uint32_le(s, 0x00020000);
    // [range(0, 65536)] unsigned long cBytes;
    out_uint32_le(s, bytes_groups);
    // [unique] [size_is(cBytes)] const byte *mszGroups; (pointer)
    out_uint32_le(s, val);
    // long fmszReadersIsNULL;
    out_uint32_le(s, 0x00000000);
    // unsigned long cchReaders;
    out_uint32_le(s, cchReaders);

    // At the end of the struct come the pointed-to structures

    // Context field pbContext is a Uni-dimensional conformant array
    out_uint32_le(s, context_bytes);
    out_uint8a(s, context, context_bytes);

    // mszGroups is also a Uni-dimensional conformant array of bytes
    if (bytes_groups > 0)
    {
        align_s(s, 4);
        out_uint32_le(s, bytes_groups);
        if (wide)
        {
            out_utf8_as_utf16_le(s, groups, groups_len);
            out_uint16_le(s, 0);
            out_uint16_le(s, 0);
        }
        else
        {
            out_uint8p(s, groups, groups_len);
            out_uint8(s, 0);
            out_uint8(s, 0);
        }
    }

    s_mark_end(s);

    s_pop_layer(s, mcs_hdr);
    bytes = (int) (s->end - s->p);
    bytes -= 8;
    out_uint32_le(s, bytes);

    s_pop_layer(s, iso_hdr);
    bytes = (int) (s->end - s->p);
    bytes -= 28;
    out_uint32_le(s, bytes);

    bytes = (int) (s->end - s->data);

    /* send to client */
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);

    LOG_DEVEL_HEXDUMP(LOG_LEVEL_TRACE, "scard_send_ListReaders:", s->data, bytes);

    free_stream(s);
}

/*****************************************************************************/
/**
 * Outputs the pointed-to-data for one of these IDL pointer types:-
 *     [string] const wchar_t* str;  (wide != 0)
 *     [string] const char* str;     (wide == 0)
 *
 * It is assumed that the referent identifier for the string has already
 * been sent
 *
 * @param s Output stream
 * @param str UTF-8 string to output
 * @param wide Whether to output as a wide string or ASCII
 *
 * Note that wchar_t on Windows is 16-bit
 * TODO: These strings have two terminators. Is this necessary?
 */
static void
out_conformant_and_varying_string(struct stream *s, const char *str, int wide)
{
    align_s(s, 4);
    unsigned int len = strlen(str);
    if (wide)
    {
        unsigned int num_chars = utf8_as_utf16_word_count(str, len);
        // Max number, offset and actual count
        out_uint32_le(s, num_chars + 2);
        out_uint32_le(s, 0);
        out_uint32_le(s, num_chars + 2);
        out_utf8_as_utf16_le(s, str, len);
        out_uint16_le(s, 0);  // Terminate string
        out_uint16_le(s, 0); // ?
    }
    else
    {
        out_uint32_le(s, len + 2);
        out_uint32_le(s, 0);
        out_uint32_le(s, len + 2);
        out_uint8p(s, str, len);
        out_uint8(s, 0);
        out_uint8(s, 0);
    }
}

/**
 * Get change in status
 *
 * @param  irp          I/O resource pkt
 * @param  wide         TRUE if unicode string
 * @param  timeout      timeout in milliseconds, -1 for infinity
 * @param  num_readers  number of entries in rsa
 * @param  rsa          array of READER_STATEs
 *****************************************************************************/
static void
scard_send_GetStatusChange(IRP *irp, char *context, int context_bytes,
                           int wide, tui32 timeout,
                           tui32 num_readers, READER_STATE *rsa)
{
    /* see [MS-RDPESC] 2.2.2.11 for ASCII
     * see [MS-RDPESC] 2.2.2.12 for Wide char
     *
     * Here is a breakdown of the Wide-char variant
     *
     * IDL:-
     *
     * typedef struct _REDIR_SCARDCONTEXT {
     *    [range(0,16)] unsigned long cbContext;
     *    [unique] [size_is(cbContext)] byte *pbContext;
     *    } REDIR_SCARDCONTEXT;
     *
     * typedef struct _ReaderState_Common_Call {
     *    unsigned long dwCurrentState;
     *    unsigned long dwEventState;
     *    [range(0,36)] unsigned long cbAtr;
     *    byte rgbAtr[36];
     *    } ReaderState_Common_Call;
     *
     * typedef struct _ReaderStateW {
     *   [string] const wchar_t* szReader;
     *   ReaderState_Common_Call Common;
     *   } ReaderStateW;
     *
     * struct _GetStatusChangeW_Call {
     *    REDIR_SCARDCONTEXT Context;
     *    unsigned long dwTimeOut;
     *    [range(0,11)] unsigned long cReaders;
     *    [size_is(cReaders)] ReaderStateW* rgReaderStates;
     *    } GetStatusChangeW_Call;
     *
     * Type summary:-
     *
     * Context.cbContext  Unsigned 32-bit word
     * Context.pbContext  Embedded full pointer to conformant array of bytes
     * dwTimeOut          Unsigned 32-bit word
     * cReaders           Unsigned 32-bit word
     * rgReaderStates
     *                    Embedded full pointer to array of rgReaderStates
     * rgReaderStates.szReader
     *                    Embedded full pointer to conformant and varying
     *                    string of [Windows] wchar_t
     * rgReaderStates.Common.dwCurrentState
     *                    Unsigned 32-bit word
     * rgReaderStates.Common.dwEventState
     *                    Unsigned 32-bit word
     * rgReaderStates.Common.cbAtr
     *                    Unsigned 32-bit word
     * rgReaderStates.Common.rgbAtr[36]
     *                    Uni-dimensional fixed array
     *
     * NDL:-
     * Offset   Decription
     * 0        Context.cbContext
     * 4        Referent Identifier for pbContext
     * 8        dwTimeOut;
     * 12       cReaders;
     * 16       Referent Identifier for rgReaderStates
     * 20       Conformant Array pointed to by pbContext
     * ??       Conformant Array pointed to by rgReaderStates. Each element
     *          of this array has a pointer to a string for the name
     * ??       String names pointed to in the above array.
     */

    SMARTCARD     *sc;
    READER_STATE  *rs;
    struct stream *s;
    tui32          ioctl;
    int            bytes;
    unsigned int   i;

    if ((sc = smartcards[irp->scard_index]) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "smartcards[%d] is NULL", irp->scard_index);
        return;
    }

    ioctl = (wide) ? SCARD_IOCTL_GET_STATUS_CHANGE_W :
            SCARD_IOCTL_GET_STATUS_CHANGE_A;

    if ((s = scard_make_new_ioctl(irp, ioctl)) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "scard_make_new_ioctl failed");
        return;
    }

    s_push_layer(s, mcs_hdr, 4); /* bytes, set later */
    out_uint32_le(s, 0x00000000);
    // REDIR_SCARDCONTEXT Context;
    out_uint32_le(s, context_bytes);
    out_uint32_le(s, 0x00020000);

    // unsigned long dwTimeOut;
    out_uint32_le(s, timeout);
    // [range(0,11)] unsigned long cReaders;
    out_uint32_le(s, num_readers);
    // [size_is(cReaders)] ReaderStateW* rgReaderStates;
    out_uint32_le(s, 0x00020004);

    // At the end of the struct come the pointed-to structures

    // Context field pbContext is a Uni-dimensional conformant array
    out_uint32_le(s, context_bytes);
    out_uint8a(s, context, context_bytes);

    // rgReaderState is a Uni-dimensional conformant array
    align_s(s, 4);
    out_uint32_le(s, num_readers);

    /* insert card reader state */
    for (i = 0; i < num_readers; i++)
    {
        rs = &rsa[i];
        //  [string] const wchar_t* szReader (wide)
        //  [string] const char_t* szReader (ASCII)
        out_uint32_le(s, 0x00020008 + (i * 4));
        //  unsigned long dwCurrentState;
        out_uint32_le(s, rs->current_state);
        //  unsigned long dwEventState;
        out_uint32_le(s, rs->event_state);
        //  [range(0,36)] unsigned long cbAtr;
        out_uint32_le(s, rs->atr_len);
        //  byte rgbAtr[36];
        out_uint8p(s, rs->atr, 33);
        out_uint8s(s, 3);
    }

    /* insert card reader names */
    for (i = 0; i < num_readers; i++)
    {
        rs = &rsa[i];
        out_conformant_and_varying_string(s, rs->reader_name, wide);
    }

    s_mark_end(s);

    s_pop_layer(s, mcs_hdr);
    bytes = (int) (s->end - s->p);
    bytes -= 8;
    out_uint32_le(s, bytes);

    s_pop_layer(s, iso_hdr);
    bytes = (int) (s->end - s->p);
    bytes -= 28;
    out_uint32_le(s, bytes);

    bytes = (int) (s->end - s->data);

    /* send to client */
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);

    LOG_DEVEL_HEXDUMP(LOG_LEVEL_TRACE, "scard_send_GetStatusChange:", s->data, bytes);

    free_stream(s);
}

/**
 * Send connect command
 *
 * @param  irp  I/O resource pkt
 * @param  wide TRUE if unicode string
 * @param  rs   reader state
 *****************************************************************************/
static void
scard_send_Connect(IRP *irp, char *context, int context_bytes,
                   int wide, READER_STATE *rs)
{
    /* see [MS-RDPESC] 2.2.2.13 for ASCII
     * see [MS-RDPESC] 2.2.2.14 for Wide char
     *
     * Here is a breakdown of the Wide-char variant
     *
     * IDL:-
     *
     * typedef struct _REDIR_SCARDCONTEXT {
     *    [range(0,16)] unsigned long cbContext;
     *    [unique] [size_is(cbContext)] byte *pbContext;
     *    } REDIR_SCARDCONTEXT;
     *
     * typedef struct _Connect_Common {
     *     REDIR_SCARDCONTEXT Context;
     *     unsigned long dwShareMode;
     *     unsigned long dwPreferredProtocols;
     * } Connect_Common;
     *
     * typedef struct _ConnectW_Call {
     *     [string] const wchar_t* szReader;
     *     Connect_Common Common;
     * } ConnectW_Call;
     *
     * Type summary:-
     *
     * szReader           Embedded full pointer to conformant and varying
     *                    string of [Windows] wchar_t
     * Common.Context.cbContext
     *                    Unsigned 32-bit word
     * Common.Context.pbContext
     *                    Embedded full pointer to conformant array of bytes
     * Common.dwShareMode Unsigned 32-bit word
     * Common.dwPreferredProtocols
     *                    Unsigned 32-bit word
     *
     * NDL:-
     *
     * Offset   Decription
     * 0        Referent Identifier for szReader
     * 4        Context.cbContext
     * 8        Referent Identifier for pbContext
     * 12       dwShareMode
     * 16       dwPreferredProtocols
     * 20       Conformant Array pointed to by szReader
     * ??       Conformant Array pointed to by pbContext
     *
     */
    SMARTCARD     *sc;
    struct stream *s;
    tui32          ioctl;
    int            bytes;

    if ((sc = smartcards[irp->scard_index]) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "smartcards[%d] is NULL", irp->scard_index);
        return;
    }

    ioctl = (wide) ? SCARD_IOCTL_CONNECT_W :
            SCARD_IOCTL_CONNECT_A;

    if ((s = scard_make_new_ioctl(irp, ioctl)) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "scard_make_new_ioctl failed");
        return;
    }

    s_push_layer(s, mcs_hdr, 4); /* bytes, set later */
    out_uint32_le(s, 0x00000000);
    // [string] const wchar_t* szReader;
    out_uint32_le(s, 0x00020000);

    // REDIR_SCARDCONTEXT Context;
    out_uint32_le(s, context_bytes);
    out_uint32_le(s, 0x00020004);

    // unsigned long dwShareMode;
    out_uint32_le(s, rs->dwShareMode);
    // unsigned long dwPreferredProtocols;
    out_uint32_le(s, rs->dwPreferredProtocols);

    /* insert card reader name */
    out_conformant_and_varying_string(s, rs->reader_name, wide);

    /* insert context */
    align_s(s, 4);
    out_uint32_le(s, context_bytes);
    out_uint8a(s, context, context_bytes);
    out_uint32_le(s, 0); // ?

    s_mark_end(s);

    s_pop_layer(s, mcs_hdr);
    bytes = (int) (s->end - s->p);
    bytes -= 8;
    out_uint32_le(s, bytes);

    s_pop_layer(s, iso_hdr);
    bytes = (int) (s->end - s->p);
    bytes -= 28;
    out_uint32_le(s, bytes);

    bytes = (int) (s->end - s->data);

    /* send to client */
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);

    free_stream(s);
}

/**
 * The reconnect method re-establishes a smart card reader handle. On success,
 * the handle is valid once again.
 *
 * @param  con        connection to client
 * @param  sc_handle  handle to device
 * @param  rs         reader state where following fields are set
 *                        rs.shared_mode_flag
 *                        rs.preferred_protocol
 *                        rs.init_type
 *****************************************************************************/
static void
scard_send_Reconnect(IRP *irp, char *context, int context_bytes,
                     char *card, int card_bytes, READER_STATE *rs)
{
    /* see [MS-RDPESC] 2.2.2.15 */
    /* see [MS-RDPESC] 3.1.4.36 */

    SMARTCARD     *sc;
    struct stream *s;
    int            bytes;

    if ((sc = smartcards[irp->scard_index]) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "smartcards[%d] is NULL", irp->scard_index);
        return;
    }

    if ((s = scard_make_new_ioctl(irp, SCARD_IOCTL_RECONNECT)) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "scard_make_new_ioctl failed");
        return;
    }

    /*
     * command format
     *
     * ......
     *       20 bytes    padding
     * u32    4 bytes    len 8, LE, v1
     * u32    4 bytes    filler
     *       24 bytes    unused (s->p currently pointed here at unused[0])
     * u32    4 bytes    dwShareMode
     * u32    4 bytes    dwPreferredProtocols
     * u32    4 bytes    dwInitialization
     * u32    4 bytes    context length
     * u32    4 bytes    context
     * u32    4 bytes    handle length
     * u32    4 bytes    handle
     */

    xstream_seek(s, 24); /* TODO */

    out_uint32_le(s, rs->dwShareMode);
    out_uint32_le(s, rs->dwPreferredProtocols);
    out_uint32_le(s, rs->init_type);
    out_uint32_le(s, context_bytes);
    out_uint8a(s, context, context_bytes);
    out_uint32_le(s, card_bytes);
    out_uint8a(s, card, card_bytes);

    s_mark_end(s);

    s_pop_layer(s, iso_hdr);
    bytes = (int) (s->end - s->p);
    bytes -= 28;
    out_uint32_le(s, bytes);

    bytes = (int) (s->end - s->data);

    /* send to client */
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);

    free_stream(s);
}

/**
 * Lock smart card reader for exclusive access for specified smart
 * card reader handle.
 *
 * @param  con   connection to client
 *****************************************************************************/
static void
scard_send_BeginTransaction(IRP *irp, char *context, int context_bytes,
                            char *card, int card_bytes)
{
    /* see [MS-RDPESC] 4.9 */

    SMARTCARD     *sc;
    struct stream *s;
    int            bytes;

    if ((sc = smartcards[irp->scard_index]) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "smartcards[%d] is NULL", irp->scard_index);
        return;
    }

    if ((s = scard_make_new_ioctl(irp, SCARD_IOCTL_BEGIN_TRANSACTION)) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "scard_make_new_ioctl failed");
        return;
    }

    s_push_layer(s, mcs_hdr, 4); /* bytes, set later */
    out_uint32_le(s, 0x00000000);
    out_uint32_le(s, context_bytes);
    out_uint32_le(s, 0x00020000);
    out_uint32_le(s, card_bytes);
    out_uint32_le(s, 0x00020004);
    out_uint32_le(s, 0x00000000);

    /* insert context */
    out_uint32_le(s, context_bytes);
    out_uint8a(s, context, context_bytes);

    /* insert card */
    out_uint32_le(s, card_bytes);
    out_uint8a(s, card, card_bytes);

    out_uint32_le(s, 0x00000000);

    s_mark_end(s);

    s_pop_layer(s, mcs_hdr);
    bytes = (int) (s->end - s->p);
    bytes -= 8;
    out_uint32_le(s, bytes);

    s_pop_layer(s, iso_hdr);
    bytes = (int) (s->end - s->p);
    bytes -= 28;
    out_uint32_le(s, bytes);

    bytes = (int) (s->end - s->data);

    /* send to client */
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);

    free_stream(s);
}

/**
 * Release a smart card reader after being locked by a previously
 * successful call to Begin Transaction
 *
 * @param  con        connection to client
 * @param  sc_handle  handle to smartcard
 *****************************************************************************/
static void
scard_send_EndTransaction(IRP *irp, char *context, int context_bytes,
                          char *card, int card_bytes,
                          tui32 dwDisposition)
{
    /* see [MS-RDPESC] 3.1.4.32 */

    SMARTCARD     *sc;
    struct stream *s;
    int            bytes;

    if ((sc = smartcards[irp->scard_index]) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "smartcards[%d] is NULL", irp->scard_index);
        return;
    }

    if ((s = scard_make_new_ioctl(irp, SCARD_IOCTL_END_TRANSACTION)) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "scard_make_new_ioctl failed");
        return;
    }

    s_push_layer(s, mcs_hdr, 4); /* bytes, set later */
    out_uint32_le(s, 0x00000000);
    out_uint32_le(s, context_bytes);
    out_uint32_le(s, 0x00020000);
    out_uint32_le(s, card_bytes);
    out_uint32_le(s, 0x00020004);
    out_uint32_le(s, dwDisposition);

    /* insert context */
    out_uint32_le(s, context_bytes);
    out_uint8a(s, context, context_bytes);

    /* insert card */
    out_uint32_le(s, card_bytes);
    out_uint8a(s, card, card_bytes);

    out_uint32_le(s, 0);

    s_mark_end(s);

    s_pop_layer(s, mcs_hdr);
    bytes = (int) (s->end - s->p);
    bytes -= 8;
    out_uint32_le(s, bytes);

    s_pop_layer(s, iso_hdr);
    bytes = (int) (s->end - s->p);
    bytes -= 28;
    out_uint32_le(s, bytes);

    bytes = (int) (s->end - s->data);

    /* send to client */
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);

    free_stream(s);
}

/**
 * Get the status of a connection for a valid smart card reader handle
 *
 * @param  con   connection to client
 * @param  wide  TRUE if unicode string
 *****************************************************************************/
static void
scard_send_Status(IRP *irp, int wide, char *context, int context_bytes,
                  char *card, int card_bytes,
                  int cchReaderLen, int cbAtrLen)
{
    /* see [MS-RDPESC] 2.2.2.18 */

    SMARTCARD     *sc;
    struct stream *s;
    int            bytes;
    tui32          ioctl;

    if ((sc = smartcards[irp->scard_index]) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "smartcards[%d] is NULL", irp->scard_index);
        return;
    }

    ioctl = wide ? SCARD_IOCTL_STATUS_W : SCARD_IOCTL_STATUS_A;
    if ((s = scard_make_new_ioctl(irp, ioctl)) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "scard_make_new_ioctl");
        return;
    }
    /*
              30 00 00 00
              00 00 00 00
              04 00 00 00
              00 00 02 00
              04 00 00 00
              04 00 02 00
              01 00 00 00
              00 00 00 00 dwReaderLen
              40 00 00 00 dwAtrLen
              04 00 00 00
              07 00 00 00
              04 00 00 00
              09 00 00 00 hCard
              00 00 00 00
    */
    s_push_layer(s, mcs_hdr, 4); /* bytes, set later */
    out_uint32_le(s, 0x00000000);
    out_uint32_le(s, context_bytes);
    out_uint32_le(s, 0x00020000);
    out_uint32_le(s, card_bytes);
    out_uint32_le(s, 0x00020004);
    out_uint32_le(s, 0x00000001);
    out_uint32_le(s, cchReaderLen); /* readerLen, see [MS-RDPESC] 4.11 */
    out_uint32_le(s, cbAtrLen); /* atrLen,    see [MS-RDPESC] 4.11 */

    /* insert context */
    out_uint32_le(s, context_bytes);
    out_uint8a(s, context, context_bytes);

    /* insert card */
    out_uint32_le(s, card_bytes);
    out_uint8a(s, card, card_bytes);

    out_uint32_le(s, 0);

    s_mark_end(s);

    s_pop_layer(s, mcs_hdr);
    bytes = (int) (s->end - s->p);
    bytes -= 8;
    out_uint32_le(s, bytes);

    s_pop_layer(s, iso_hdr);
    bytes = (int) (s->end - s->p);
    bytes -= 28;
    out_uint32_le(s, bytes);

    bytes = (int) (s->end - s->data);

    LOG_DEVEL_HEXDUMP(LOG_LEVEL_TRACE, "", s->data, bytes);

    /* send to client */
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);

    free_stream(s);
}

/**
 * Release a smart card reader handle that was acquired in ConnectA/ConnectW
 *
 * @param  con        connection to client
 * @param  sc_handle  handle to smartcard
 *****************************************************************************/
static void
scard_send_Disconnect(IRP *irp, char *context, int context_bytes,
                      char *card, int card_bytes, int dwDisposition)
{
    /* see [MS-RDPESC] 3.1.4.30 */

    SMARTCARD     *sc;
    struct stream *s;
    int            bytes;

    if ((sc = smartcards[irp->scard_index]) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "smartcards[%d] is NULL", irp->scard_index);
        return;
    }

    if ((s = scard_make_new_ioctl(irp, SCARD_IOCTL_DISCONNECT)) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "scard_make_new_ioctl failed");
        return;
    }

    s_push_layer(s, mcs_hdr, 4); /* bytes, set later */
    out_uint32_le(s, 0x00000000);
    out_uint32_le(s, context_bytes);
    out_uint32_le(s, 0x00020000);
    out_uint32_le(s, card_bytes);
    out_uint32_le(s, 0x00020004);
    out_uint32_le(s, dwDisposition);

    /* insert context */
    out_uint32_le(s, context_bytes);
    out_uint8a(s, context, context_bytes);

    /* insert card */
    out_uint32_le(s, card_bytes);
    out_uint8a(s, card, card_bytes);

    out_uint32_le(s, 0x00000000);

    s_mark_end(s);

    s_pop_layer(s, mcs_hdr);
    bytes = (int) (s->end - s->p);
    bytes -= 8;
    out_uint32_le(s, bytes);

    s_pop_layer(s, iso_hdr);
    bytes = (int) (s->end - s->p);
    bytes -= 28;
    out_uint32_le(s, bytes);

    bytes = (int) (s->end - s->data);

    /* send to client */
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);

    free_stream(s);
}

/**
 * The Transmit_Call structure is used to send data to the smart card
 * associated with a valid context.
 *****************************************************************************/
static int
scard_send_Transmit(IRP *irp, char *context, int context_bytes,
                    char *card, int card_bytes, char *send_data,
                    int send_bytes, int recv_bytes,
                    struct xrdp_scard_io_request *send_ior,
                    struct xrdp_scard_io_request *recv_ior)
{
    /* see [MS-RDPESC] 2.2.2.19 */

    SMARTCARD     *sc;
    struct stream *s;
    int            bytes;
    int            val;

    if ((sc = smartcards[irp->scard_index]) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "smartcards[%d] is NULL", irp->scard_index);
        return 1;
    }

    if ((s = scard_make_new_ioctl(irp, SCARD_IOCTL_TRANSMIT)) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "scard_make_new_ioctl");
        return 1;
    }

    LOG_DEVEL(LOG_LEVEL_DEBUG, "send_bytes %d recv_bytes %d send dwProtocol %d cbPciLength %d "
              "extra_bytes %d recv dwProtocol %d cbPciLength %d extra_bytes %d",
              send_bytes, recv_bytes, send_ior->dwProtocol, send_ior->cbPciLength,
              send_ior->extra_bytes, recv_ior->dwProtocol, recv_ior->cbPciLength,
              recv_ior->extra_bytes);

    /*
     * command format
     *
     * ......
     *       20 bytes    padding
     * u32    4 bytes    len 8, LE, v1
     * u32    4 bytes    filler
     *       12 bytes    unused (s->p currently pointed here at unused[0])
     * u32    4 bytes    map0
     *        4 bytes    unused
     * u32    4 bytes    map1
     * u32    4 bytes    dwProtocol
     * u32    4 bytes    cbPciLength
     * u32    4 bytes    map2
     * u32    4 bytes    cbSendLength
     * u32    4 bytes    map3
     * u32    4 bytes    map4
     * u32    4 bytes    map5
     * u32    4 bytes    map6
     * u32    4 bytes    cbRecvLength
     * u32    4 bytes    len of sc_handle
     * u32    4 bytes    sc_handle
     */

    //g_writeln("send_bytes %d", send_bytes);
    //g_writeln("recv_bytes %d", recv_bytes);

#if 0
    s_push_layer(s, mcs_hdr, 4); /* bytes, set later */
    out_uint32_be(s, 0x00000000);
    out_uint32_be(s, 0x04000000);
    out_uint32_be(s, 0x00000200); // map 0
    out_uint32_be(s, 0x04000000);
    out_uint32_be(s, 0x04000200); // map 1
    out_uint32_be(s, 0x01000000);
    out_uint32_be(s, 0x00000000);
    out_uint32_be(s, 0x00000000);

    //out_uint32_be(s, 0x05000000);
    out_uint32_le(s, send_bytes);

    out_uint32_be(s, 0x08000200);
    out_uint32_be(s, 0x0c000200);
    out_uint32_be(s, 0x00000000);

    //out_uint32_be(s, 0x02010000);
    out_uint32_le(s, recv_bytes);

    out_uint32_be(s, 0x04000000);
    out_uint32_be(s, 0x05000000);
    out_uint32_be(s, 0x04000000);
    out_uint32_be(s, 0x0b000000);

    //out_uint32_be(s, 0x05000000);
    //out_uint32_be(s, 0x00b00704);
    //out_uint32_be(s, 0x10000000);
    out_uint32_le(s, send_bytes);
    out_uint8p(s, send_data, send_bytes);
    align_s(s, 4);

    out_uint32_be(s, 0x01000000);
    out_uint32_be(s, 0x00000000);
    out_uint32_be(s, 0x00000000);
#else

    //g_printf("send cbPciLength %d\n", send_ior->cbPciLength);
    //g_printf("send extra_bytes %d\n", send_ior->extra_bytes);
    //g_printf("recv cbPciLength %d\n", recv_ior->cbPciLength);
    //g_printf("recv extra_bytes %d\n", recv_ior->extra_bytes);

    s_push_layer(s, mcs_hdr, 4); /* bytes, set later */
    out_uint32_le(s, 0x00000000);

    out_uint32_le(s, context_bytes);
    out_uint32_le(s, 0x00020000); /* map0 */

    out_uint32_le(s, card_bytes);
    out_uint32_le(s, 0x00020004); /* map1 */

    out_uint32_le(s, send_ior->dwProtocol);
    out_uint32_le(s, send_ior->cbPciLength - 8);

    val = send_ior->extra_bytes > 0 ? 1 : 0;
    out_uint32_le(s, val); /* map2 */

    out_uint32_le(s, send_bytes);

    val = send_bytes > 0 ? 0x00020008 : 0;
    out_uint32_le(s, val); /* map3 */

    val = recv_ior->cbPciLength > 0 ? 0x0002000c : 0;
    out_uint32_le(s, val); /* map 4 */

    out_uint32_le(s, 0); // map5
    out_uint32_le(s, recv_bytes);

    /* map0 */
    out_uint32_le(s, context_bytes);
    out_uint8a(s, context, context_bytes);

    /* map1 */
    out_uint32_le(s, card_bytes);
    out_uint8a(s, card, card_bytes);

    if (send_ior->extra_bytes > 0)
    {
        out_uint32_le(s, send_ior->extra_bytes);
        out_uint8a(s, send_ior->extra_data, send_ior->extra_bytes);
    }

    if (send_bytes > 0)
    {
        out_uint32_le(s, send_bytes);
        out_uint8a(s, send_data, send_bytes);
        align_s(s, 4);
    }

    if (recv_ior->cbPciLength > 0)
    {
        /* map4 */
        out_uint32_le(s, recv_ior->dwProtocol);
        out_uint32_le(s, recv_ior->cbPciLength - 8);
        val = recv_ior->extra_bytes > 0 ? 1 : 0;
        out_uint32_le(s, val); /* map6*/
        if (val)
        {
            out_uint32_le(s, recv_ior->extra_bytes);
            out_uint8a(s, recv_ior->extra_data, recv_ior->extra_bytes);
        }
    }
#endif

    s_mark_end(s);

    s_pop_layer(s, mcs_hdr);
    bytes = (int) (s->end - s->p);
    bytes -= 8;
    out_uint32_le(s, bytes);

    s_pop_layer(s, iso_hdr);
    bytes = (int) (s->end - s->p);
    bytes -= 28;
    out_uint32_le(s, bytes);

    bytes = (int) (s->end - s->data);

    /* send to client */
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);

    LOG_DEVEL_HEXDUMP(LOG_LEVEL_TRACE, "scard_send_Transmit:", s->data, bytes);

    free_stream(s);
    return 0;
}

/**
 * Communicate directly with the smart card reader
 *****************************************************************************/
static int
scard_send_Control(IRP *irp, char *context, int context_bytes,
                   char *card, int card_bytes, char *send_data,
                   int send_bytes, int recv_bytes, int control_code)
{
    /* see [MS-RDPESC] 2.2.2.19 */

    SMARTCARD     *sc;
    struct stream *s;
    int            bytes;
    int            val;

    if ((sc = smartcards[irp->scard_index]) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "smartcards[%d] is NULL", irp->scard_index);
        return 1;
    }

    if ((s = scard_make_new_ioctl(irp, SCARD_IOCTL_CONTROL)) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "scard_make_new_ioctl");
        return 1;
    }

    s_push_layer(s, mcs_hdr, 4); /* bytes, set later */
    out_uint32_le(s, 0x00000000);
    out_uint32_le(s, context_bytes);
    out_uint32_le(s, 0x00020000); /* map0 */
    out_uint32_le(s, card_bytes);
    out_uint32_le(s, 0x00020004); /* map1 */
    out_uint32_le(s, control_code);
    out_uint32_le(s, send_bytes);
    val = send_bytes > 0 ? 0x00020008 : 0;
    out_uint32_le(s, val);        /* map2 */
    out_uint32_le(s, 0x00000000);
    out_uint32_le(s, recv_bytes);
    out_uint32_le(s, context_bytes);
    out_uint8a(s, context, context_bytes);
    out_uint32_le(s, card_bytes);
    out_uint8a(s, card, card_bytes);
    if (send_bytes > 0)
    {
        out_uint32_le(s, send_bytes);
        out_uint8a(s, send_data, send_bytes);
    }
    else
    {
        out_uint32_le(s, 0x00000000);
    }

    s_mark_end(s);

    s_pop_layer(s, mcs_hdr);
    bytes = (int) (s->end - s->p);
    bytes -= 8;
    out_uint32_le(s, bytes);

    s_pop_layer(s, iso_hdr);
    bytes = (int) (s->end - s->p);
    bytes -= 28;
    out_uint32_le(s, bytes);

    bytes = (int) (s->end - s->data);

    LOG_DEVEL_HEXDUMP(LOG_LEVEL_TRACE, "", s->data, bytes);

    /* send to client */
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);

    free_stream(s);
    return 0;
}

/**
 * Cancel any outstanding calls
 *****************************************************************************/
static int
scard_send_Cancel(IRP *irp, char *context, int context_bytes)
{
    /* see [MS-RDPESC] 3.1.4.27 */

    SMARTCARD     *sc;
    struct stream *s;
    int            bytes;

    if ((sc = smartcards[irp->scard_index]) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "smartcards[%d] is NULL", irp->scard_index);
        return 1;
    }

    if ((s = scard_make_new_ioctl(irp, SCARD_IOCTL_CANCEL)) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "scard_make_new_ioctl");
        return 1;
    }

    s_push_layer(s, mcs_hdr, 4); /* bytes, set later */
    out_uint32_le(s, 0x00000000);
    out_uint32_le(s, context_bytes);
    out_uint32_le(s, 0x00020000);
    out_uint32_le(s, context_bytes);
    out_uint8a(s, context, context_bytes);

    s_mark_end(s);

    s_pop_layer(s, mcs_hdr);
    bytes = (int) (s->end - s->p);
    bytes -= 8;
    out_uint32_le(s, bytes);

    s_pop_layer(s, iso_hdr);
    bytes = (int) (s->end - s->p);
    bytes -= 28;
    out_uint32_le(s, bytes);

    bytes = (int) (s->end - s->data);

    /* send to client */
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);

    free_stream(s);
    return 0;
}

/**
 * Get reader attributes
 *****************************************************************************/
static int
scard_send_GetAttrib(IRP *irp, char *card, int card_bytes, READER_STATE *rs)
{
    /* see [MS-RDPESC] 2.2.2.21 */

    SMARTCARD     *sc;
    struct stream *s;
    int            bytes;

    if ((sc = smartcards[irp->scard_index]) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "smartcards[%d] is NULL", irp->scard_index);
        return 1;
    }

    if ((s = scard_make_new_ioctl(irp, SCARD_IOCTL_GETATTRIB)) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "scard_make_new_ioctl");
        return 1;
    }

    /*
     * command format
     *
     * ......
     *       20 bytes    padding
     * u32    4 bytes    len 8, LE, v1
     * u32    4 bytes    filler
     *       24 bytes    unused (s->p currently pointed here at unused[0])
     * u32    4 bytes    dwAttribId
     *        4 bytes    unused
     * u32    4 bytes    dwAttrLen
     *        8 bytes    unused
     * u32    4 bytes    handle len
     * u32    4 bytes    handle
     */

    xstream_seek(s, 24); /* TODO */
    out_uint32_le(s, rs->dwAttribId);
    out_uint32_le(s, 0);
    out_uint32_le(s, rs->dwAttrLen);
    xstream_seek(s, 8);
    out_uint32_le(s, card_bytes);
    out_uint8a(s, card, card_bytes);

    s_mark_end(s);

    s_pop_layer(s, iso_hdr);
    bytes = (int) (s->end - s->p);
    bytes -= 28;
    out_uint32_le(s, bytes);

    bytes = (int) (s->end - s->data);

    /* send to client */
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);

    free_stream(s);
    return 0;
}

/******************************************************************************
**                                                                           **
**                    local callbacks into this module                       **
**                                                                           **
******************************************************************************/

/**
 *
 *****************************************************************************/
static void
scard_handle_EstablishContext_Return(struct stream *s, IRP *irp,
                                     tui32 DeviceId, tui32 CompletionId,
                                     tui32 IoStatus)
{
    tui32 len;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "entered");
    /* sanity check */
    if ((DeviceId != irp->DeviceId) || (CompletionId != irp->CompletionId))
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "DeviceId/CompletionId do not match those in IRP");
        return;
    }
    /* get OutputBufferLen */
    xstream_rd_u32_le(s, len);
    scard_function_establish_context_return(irp->user_data, s, len, IoStatus);
    devredir_irp_delete(irp);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "leaving");
}

/**
 *
 *****************************************************************************/
static void
scard_handle_ReleaseContext_Return(struct stream *s, IRP *irp,
                                   tui32 DeviceId, tui32 CompletionId,
                                   tui32 IoStatus)
{
    tui32 len;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "entered");
    /* sanity check */
    if ((DeviceId != irp->DeviceId) || (CompletionId != irp->CompletionId))
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "DeviceId/CompletionId do not match those in IRP");
        return;
    }
    /* get OutputBufferLen */
    xstream_rd_u32_le(s, len);
    scard_function_release_context_return(irp->user_data, s, len, IoStatus);
    devredir_irp_delete(irp);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "leaving");
}

/**
 *
 *****************************************************************************/
static void
scard_handle_IsContextValid_Return(struct stream *s, IRP *irp,
                                   tui32 DeviceId, tui32 CompletionId,
                                   tui32 IoStatus)
{
    tui32 len;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "entered");

    /* sanity check */
    if ((DeviceId != irp->DeviceId) || (CompletionId != irp->CompletionId))
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "DeviceId/CompletionId do not match those in IRP");
        return;
    }

    /* get OutputBufferLen */
    xstream_rd_u32_le(s, len);
    scard_function_is_context_valid_return(irp->user_data, s, len, IoStatus);
    devredir_irp_delete(irp);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "leaving");
}

/**
 *
 *****************************************************************************/
static void
scard_handle_ListReaders_Return(struct stream *s, IRP *irp,
                                tui32 DeviceId, tui32 CompletionId,
                                tui32 IoStatus)
{
    tui32 len;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "entered");
    /* sanity check */
    if ((DeviceId != irp->DeviceId) || (CompletionId != irp->CompletionId))
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "DeviceId/CompletionId do not match those in IRP");
        return;
    }
    /* get OutputBufferLen */
    xstream_rd_u32_le(s, len);
    scard_function_list_readers_return(irp->user_data, s, len, IoStatus);
    devredir_irp_delete(irp);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "leaving");
}

/**
 *
 *****************************************************************************/
static void
scard_handle_GetStatusChange_Return(struct stream *s, IRP *irp,
                                    tui32 DeviceId, tui32 CompletionId,
                                    tui32 IoStatus)
{
    tui32 len;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "entered");
    /* sanity check */
    if ((DeviceId != irp->DeviceId) || (CompletionId != irp->CompletionId))
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "DeviceId/CompletionId do not match those in IRP");
        return;
    }
    /* get OutputBufferLen */
    xstream_rd_u32_le(s, len);
    scard_function_get_status_change_return(irp->user_data, s, len, IoStatus);
    devredir_irp_delete(irp);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "leaving");
}

/**
 *
 *****************************************************************************/
static void
scard_handle_Connect_Return(struct stream *s, IRP *irp,
                            tui32 DeviceId, tui32 CompletionId,
                            tui32 IoStatus)
{
    tui32 len;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "entered");

    /* sanity check */
    if ((DeviceId != irp->DeviceId) || (CompletionId != irp->CompletionId))
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "DeviceId/CompletionId do not match those in IRP");
        return;
    }

    /* get OutputBufferLen */
    xstream_rd_u32_le(s, len);

    scard_function_connect_return(irp->user_data, s, len, IoStatus);
    devredir_irp_delete(irp);

    LOG_DEVEL(LOG_LEVEL_DEBUG, "leaving");
}

/**
 *
 *****************************************************************************/
static void
scard_handle_Reconnect_Return(struct stream *s, IRP *irp,
                              tui32 DeviceId, tui32 CompletionId,
                              tui32 IoStatus)
{
    tui32 len;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "entered");

    /* sanity check */
    if ((DeviceId != irp->DeviceId) || (CompletionId != irp->CompletionId))
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "DeviceId/CompletionId do not match those in IRP");
        return;
    }

    /* get OutputBufferLen */
    xstream_rd_u32_le(s, len);
    scard_function_reconnect_return(irp->user_data, s, len, IoStatus);
    devredir_irp_delete(irp);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "leaving");
}

/**
 *
 *****************************************************************************/
static void
scard_handle_BeginTransaction_Return(struct stream *s, IRP *irp,
                                     tui32 DeviceId, tui32 CompletionId,
                                     tui32 IoStatus)
{
    tui32 len;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "entered");

    /* sanity check */
    if ((DeviceId != irp->DeviceId) || (CompletionId != irp->CompletionId))
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "DeviceId/CompletionId do not match those in IRP");
        return;
    }

    /* get OutputBufferLen */
    xstream_rd_u32_le(s, len);
    scard_function_begin_transaction_return(irp->user_data, s, len, IoStatus);
    devredir_irp_delete(irp);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "leaving");
}

/**
 *
 *****************************************************************************/
static void
scard_handle_EndTransaction_Return(struct stream *s, IRP *irp,
                                   tui32 DeviceId, tui32 CompletionId,
                                   tui32 IoStatus)
{
    tui32 len;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "entered");

    /* sanity check */
    if ((DeviceId != irp->DeviceId) || (CompletionId != irp->CompletionId))
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "DeviceId/CompletionId do not match those in IRP");
        return;
    }

    /* get OutputBufferLen */
    xstream_rd_u32_le(s, len);
    scard_function_end_transaction_return(irp->user_data, s, len, IoStatus);
    devredir_irp_delete(irp);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "leaving");
}

/**
 *
 *****************************************************************************/
static void
scard_handle_Status_Return(struct stream *s, IRP *irp,
                           tui32 DeviceId, tui32 CompletionId,
                           tui32 IoStatus)
{
    tui32 len;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "entered");

    /* sanity check */
    if ((DeviceId != irp->DeviceId) || (CompletionId != irp->CompletionId))
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "DeviceId/CompletionId do not match those in IRP");
        return;
    }

    /* get OutputBufferLen */
    xstream_rd_u32_le(s, len);
    scard_function_status_return(irp->user_data, s, len, IoStatus);
    devredir_irp_delete(irp);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "leaving");
}

/**
 *
 *****************************************************************************/
static void
scard_handle_Disconnect_Return(struct stream *s, IRP *irp,
                               tui32 DeviceId, tui32 CompletionId,
                               tui32 IoStatus)
{
    tui32 len;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "entered");

    /* sanity check */
    if ((DeviceId != irp->DeviceId) || (CompletionId != irp->CompletionId))
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "DeviceId/CompletionId do not match those in IRP");
        return;
    }

    /* get OutputBufferLen */
    xstream_rd_u32_le(s, len);
    scard_function_disconnect_return(irp->user_data, s, len, IoStatus);
    devredir_irp_delete(irp);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "leaving");
}

/**
 *
 *****************************************************************************/
static void
scard_handle_Transmit_Return(struct stream *s, IRP *irp, tui32 DeviceId,
                             tui32 CompletionId, tui32 IoStatus)
{
    tui32 len;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "entered");

    /* sanity check */
    if ((DeviceId != irp->DeviceId) || (CompletionId != irp->CompletionId))
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "DeviceId/CompletionId do not match those in IRP");
        return;
    }

    /* get OutputBufferLen */
    xstream_rd_u32_le(s, len);
    scard_function_transmit_return(irp->user_data, s, len, IoStatus);
    devredir_irp_delete(irp);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "leaving");
}

/**
 *
 *****************************************************************************/
static void
scard_handle_Control_Return(struct stream *s, IRP *irp, tui32 DeviceId,
                            tui32 CompletionId, tui32 IoStatus)
{
    tui32 len;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "entered");

    /* sanity check */
    if ((DeviceId != irp->DeviceId) || (CompletionId != irp->CompletionId))
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "DeviceId/CompletionId do not match those in IRP");
        return;
    }

    /* get OutputBufferLen */
    xstream_rd_u32_le(s, len);
    scard_function_control_return(irp->user_data, s, len, IoStatus);
    devredir_irp_delete(irp);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "leaving");
}

/**
 *
 *****************************************************************************/
static void
scard_handle_Cancel_Return(struct stream *s, IRP *irp, tui32 DeviceId,
                           tui32 CompletionId, tui32 IoStatus)
{
    tui32 len;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "entered");

    /* sanity check */
    if ((DeviceId != irp->DeviceId) || (CompletionId != irp->CompletionId))
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "DeviceId/CompletionId do not match those in IRP");
        return;
    }

    /* get OutputBufferLen */
    xstream_rd_u32_le(s, len);
    scard_function_cancel_return(irp->user_data, s, len, IoStatus);
    devredir_irp_delete(irp);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "leaving");
}

/**
 *
 *****************************************************************************/
static void
scard_handle_GetAttrib_Return(struct stream *s, IRP *irp, tui32 DeviceId,
                              tui32 CompletionId, tui32 IoStatus)
{
    tui32 len;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "entered");

    /* sanity check */
    if ((DeviceId != irp->DeviceId) || (CompletionId != irp->CompletionId))
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "DeviceId/CompletionId do not match those in IRP");
        return;
    }

    /* get OutputBufferLen */
    xstream_rd_u32_le(s, len);
    scard_function_get_attrib_return(irp->user_data, s, len, IoStatus);
    devredir_irp_delete(irp);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "leaving");
}
