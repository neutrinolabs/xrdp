/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Laxmikant Rashinkar 2013 LK.Rashinkar@gmail.com
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
 * smartcard redirection support
 */

#include "os_calls.h"
#include "smartcard.h"
#include "log.h"
#include "irp.h"
#include "devredir.h"

/*
 * TODO
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

/* module based logging */
#define LOG_ERROR   0
#define LOG_INFO    1
#define LOG_DEBUG   2

#ifndef LOG_LEVEL
#define LOG_LEVEL   LOG_DEBUG
#endif

#define log_error(_params...)                           \
{                                                       \
    g_write("[%10.10u]: SMART_CARD %s: %d : ERROR: ",   \
            g_time3(), __func__, __LINE__);             \
    g_writeln (_params);                                \
}

#define log_info(_params...)                            \
{                                                       \
    if (LOG_INFO <= LOG_LEVEL)                          \
    {                                                   \
        g_write("[%10.10u]: SMART_CARD %s: %d : ",      \
                g_time3(), __func__, __LINE__);         \
        g_writeln (_params);                            \
    }                                                   \
}

#define log_debug(_params...)                           \
{                                                       \
    if (LOG_DEBUG <= LOG_LEVEL)                         \
    {                                                   \
        g_write("[%10.10u]: SMART_CARD %s: %d : ",      \
                g_time3(), __func__, __LINE__);         \
        g_writeln (_params);                            \
    }                                                   \
}

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
#define SCARD_IOCTL_REMOVE_READER_FROM_GROUP 0x00090078 /* RemoveReaderFromGroup */
#define SCARD_IOCTL_GET_STATUS_CHANGE        0x000900A0 /* GetStatusChangeA     */
#define SCARD_IOCTL_CANCEL                   0x000900A8 /* Cancel               */
#define SCARD_IOCTL_CONNECT                  0x000900AC /* ConnectA             */
#define SCARD_IOCTL_RECONNECT                0x000900B4 /* Reconnect            */
#define SCARD_IOCTL_DISCONNECT               0x000900B8 /* Disconnect           */
#define SCARD_IOCTL_BEGIN_TRANSACTION        0x000900BC /* BeginTransaction     */
#define SCARD_IOCTL_END_TRANSACTION          0x000900C0 /* EndTransaction       */
#define SCARD_IOCTL_STATE                    0x000900C4 /* State                */
#define SCARD_IOCTL_STATUS                   0x000900C8 /* StatusA              */
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

#define MAX_SMARTCARDS                       16

/* stores info about a smart card */
typedef struct smartcard
{
    tui32 DeviceId;
    char  Context[16]; /* opaque context; save as passed to us */
    int   Context_len; /* Context len in bytes                 */
} SMARTCARD;

SMARTCARD *smartcards[MAX_SMARTCARDS];
int g_smartcards_inited = 0;

extern tui32 g_completion_id;
extern int g_rdpdr_chan_id;    /* in chansrv.c */

/* forward declarations specific to this file */
static void scard_send_EstablishContext(IRP *irp);
static void scard_send_ListReaders(IRP *irp, int wide);
static struct stream *scard_make_new_ioctl(IRP *irp, tui32 ioctl);
static int  scard_add_new_device(tui32 device_id);
static int  scard_get_free_slot();
static void scard_release_resources();

/******************************************************************************
**                          non static functions                             **
******************************************************************************/

void scard_device_announce(tui32 device_id)
{
    IRP *irp;

    log_debug("entered: device_id=%d", device_id);

    if (!g_smartcards_inited)
    {
        g_memset(&smartcards, 0, sizeof(smartcards));
        g_smartcards_inited = 1;
    }

    if ((irp = devredir_irp_new()) == NULL)
    {
        log_error("system out of memory");
        return;
    }

    irp->scard_index = scard_add_new_device(device_id);
    if (irp->scard_index < 0)
    {
        log_debug("NOT adding smartcard with DeviceId=%d to list", device_id);
        devredir_irp_delete(irp);
        return;
    }

    log_debug("added smartcard with DeviceId=%d to list", device_id);

    irp->CompletionId = g_completion_id++;
    irp->DeviceId = device_id;
    irp->callback = scard_handle_EstablishContext_Return;

    scard_send_EstablishContext(irp);
    log_debug("leaving");
}

/******************************************************************************
**                       callbacks into this module                          **
******************************************************************************/

void scard_handle_EstablishContext_Return(struct stream *s, IRP *irp,
                                          tui32 DeviceId, tui32 CompletionId,
                                          tui32 IoStatus)
{
    tui32      len;
    int        tmp;
    SMARTCARD *sc;

    log_debug("entered");

    /* sanity check */
    if ((DeviceId != irp->DeviceId) || (CompletionId != irp->CompletionId))
    {
        log_error("DeviceId/CompletionId do not match those in IRP");
        return;
    }

    if (IoStatus != 0)
    {
        log_error("failed to establish context - device not usable");
        /* LK_TODO delete irp and smartcard entry */
        return;
    }

    sc = smartcards[irp->scard_index];

    /* get OutputBufferLen */
    xstream_rd_u32_le(s, len);

    /* LK_TODO */
    g_hexdump(s->p, len);

    xstream_rd_u32_le(s, tmp); /* should be len 8, LE, V1 */
    xstream_rd_u32_le(s, tmp); /* marshalling flag        */
    xstream_rd_u32_le(s, tmp); /* ?? */
    xstream_rd_u32_le(s, tmp); /* ?? */
    xstream_rd_u32_le(s, tmp); /* ?? */
    xstream_rd_u32_le(s, tmp); /* ?? */
    xstream_rd_u32_le(s, tmp); /* ?? */
    xstream_rd_u32_le(s, len); /* len of context in bytes */
    sc->Context_len = len;
    xstream_copyout(sc->Context, s, len);

    if (LOG_LEVEL == LOG_DEBUG)
    {
        log_debug("dumping context (%d bytes)", sc->Context_len);
        g_hexdump(sc->Context, sc->Context_len);
    }

    irp->callback = scard_handle_ListReaders_Return;
    scard_send_ListReaders(irp, 1);

    /* LK_TODO need to delete IRP */
    log_debug("leaving");
}

void scard_handle_ListReaders_Return(struct stream *s, IRP *irp,
                                     tui32 DeviceId, tui32 CompletionId,
                                     tui32 IoStatus)
{
    tui32 len;

    log_debug("entered");

    /* sanity check */
    if ((DeviceId != irp->DeviceId) || (CompletionId != irp->CompletionId))
    {
        log_error("DeviceId/CompletionId do not match those in IRP");
        return;
    }

    if (IoStatus != 0)
    {
        log_error("failed to list readers - device not usable");
        /* LK_TODO delete irp and smartcard entry */
        return;
    }

    /* get OutputBufferLen */
    xstream_rd_u32_le(s, len);

    /* LK_TODO */
    log_debug("dumping %d bytes", len);
    g_hexdump(s->p, len);

    log_debug("leaving");
}

/******************************************************************************
**                   static functions local to this file                     **
******************************************************************************/

/**
 *
 *****************************************************************************/

static void scard_send_EstablishContext(IRP *irp)
{
    struct stream *s;
    int            bytes;

    if ((s = scard_make_new_ioctl(irp, SCARD_IOCTL_ESTABLISH_CONTEXT)) == NULL)
        return;

    xstream_wr_u32_le(s, 0x08);               /* len                      */
    xstream_wr_u32_le(s, 0);                  /* unused                   */
    xstream_wr_u32_le(s, SCARD_SCOPE_SYSTEM); /* Ioctl specific data      */
    xstream_wr_u32_le(s, 0);                  /* don't know what this is, */
                                              /* but Win7 is sending it   */
    /* get stream len */
    bytes = xstream_len(s);

    /* InputBufferLength is number of bytes AFTER 20 byte padding         */
    *(s->data + 28) = bytes - 56;

    /* send to client */
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);
    xstream_free(s);
}

/**
 *
 *****************************************************************************/

static void scard_send_ListReaders(IRP *irp, int wide)
{
    /* see [MS-RDPESC] 2.2.2.4 */

    SMARTCARD     *sc;
    struct stream *s;
    int            bytes;
    tui32          ioctl;

    if ((sc = smartcards[irp->scard_index]) == NULL)
    {
        log_error("smartcards[%d] is NULL", irp->scard_index);
        return;
    }

    ioctl = (wide > 0) ? SCARD_IOCTL_LIST_READERS_W :
                         SCARD_IOCTL_LIST_READERS_A;

    if ((s = scard_make_new_ioctl(irp, ioctl)) == NULL)
        return;

    xstream_wr_u32_le(s, 72); /* number of bytes to follow */
    xstream_seek(s, 0x1c);    /* freerdp does not use this */

    /* insert context */
    xstream_wr_u32_le(s, sc->Context_len);
    xstream_copyin(s, sc->Context, sc->Context_len);

    xstream_wr_u32_le(s, 36); /* length of mszGroups */
    xstream_wr_u16_le(s, 0x0053);
    xstream_wr_u16_le(s, 0x0043);
    xstream_wr_u16_le(s, 0x0061);
    xstream_wr_u16_le(s, 0x0072);
    xstream_wr_u16_le(s, 0x0064);
    xstream_wr_u16_le(s, 0x0024);
    xstream_wr_u16_le(s, 0x0041);
    xstream_wr_u16_le(s, 0x006c);
    xstream_wr_u16_le(s, 0x006c);
    xstream_wr_u16_le(s, 0x0052);
    xstream_wr_u16_le(s, 0x0065);
    xstream_wr_u16_le(s, 0x0061);
    xstream_wr_u16_le(s, 0x0064);
    xstream_wr_u16_le(s, 0x0065);
    xstream_wr_u16_le(s, 0x0072);
    xstream_wr_u16_le(s, 0x0073);

    xstream_wr_u32_le(s, 0x00);

    /* get stream len */
    bytes = xstream_len(s);

    /* InputBufferLength is number of bytes AFTER 20 byte padding  */
    *(s->data + 28) = bytes - 56;

    /* send to client */
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);
    xstream_free(s);

    /*
    scard_device_control: dumping 120 bytes of data
    0000 00 08 00 00 58 00 00 00 2c 00 09 00 00 00 00 00 ....X...,.......
    0010 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
    0020 01 10 08 00 cc cc cc cc 48 00 00 00 00 00 00 00 ........H.......
    0030 04 00 00 00 00 00 02 00 24 00 00 00 04 00 02 00 ........$.......
    0040 00 00 00 00 ff ff ff ff 04 00 00 00 84 db 03 01 ................
    0050 24 00 00 00 53 00 43 00 61 00 72 00 64 00 24 00 $...S.C.a.r.d.$.
    0060 41 00 6c 00 6c 00 52 00 65 00 61 00 64 00 65 00 A.l.l.R.e.a.d.e.
    0070 72 00 73 00 00 00 00 00                         r.s.....
    scard_device_control: output_len=2048 input_len=88 ioctl_code=0x9002c
    */

    /*
        scard_device_control: dumping 120 bytes of data
        0000 00 08 00 00 80 00 00 00 14 00 09 00 00 00 00 00 ................
        0010 2e 2e 00 00 00 00 00 00 02 00 00 00 00 00 00 00 ................
        0020 01 10 08 00 cc cc cc cc 48 00 00 00 00 00 00 00 ........H.......
        0030 02 00 00 00 00 00 00 00 72 64 00 00 00 00 00 00 ........rd......
        0040 81 27 00 00 00 00 00 00 04 00 00 00 84 b3 03 01 .'..............
        0050 24 00 00 00 53 00 43 00 61 00 72 00 64 00 24 00 $...S.C.a.r.d.$.
        0060 41 00 6c 00 6c 00 52 00 65 00 61 00 64 00 65 00 A.l.l.R.e.a.d.e.
        0070 72 00 73 00 00 00 00 00                         r.s.....
        scard_device_control: output_len=2048 input_len=128 ioctl_code=0x90014
    */
}

/**
 * Crate a new stream and insert specified IOCTL
 *
 * @param  irp    information about the I/O
 * @param  ioctl  the IOCTL code
 *
 * @return stream with IOCTL inserted in it, NULL on error
 *****************************************************************************/

static struct stream *scard_make_new_ioctl(IRP *irp, tui32 ioctl)
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

    xstream_new(s, 1024 * 3);
    if (s == NULL)
    {
        log_error("system out of memory");
        return s;
    }

    devredir_insert_DeviceIoRequest(s,
                                    irp->DeviceId,
                                    irp->FileId,
                                    irp->CompletionId,
                                    IRP_MJ_DEVICE_CONTROL,
                                    0);

    xstream_wr_u32_le(s, 2048);        /* OutputBufferLength               */
    xstream_wr_u32_le(s, 0);           /* InputBufferLength - insert later */
    xstream_wr_u32_le(s, ioctl);       /* Ioctl Code                       */
    xstream_seek(s, 20);               /* padding                          */

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

static int scard_add_new_device(tui32 device_id)
{
    int        index;
    SMARTCARD *sc;

    if ((index = scard_get_free_slot()) < 0)
        return -1;

    if ((sc = g_malloc(sizeof(SMARTCARD), 1)) == NULL)
    {
        log_error("system out of memory");
        return -1;
    }

    sc->DeviceId = device_id;
    smartcards[index] = sc;

    return index;
}

/**
 * Find first unused entry in smartcards
 *
 * @return index of first unused entry in smartcards or -1 if smartcards is full
 *****************************************************************************/

static int scard_get_free_slot()
{
    int i;

    for (i = 0; i < MAX_SMARTCARDS; i++)
    {
        if (smartcards[i] == NULL)
        {
            log_debug("found free slot at index %d", i);
            return i;
        }
    }

    log_error("too many smart card devices; rejecting this one");
    return -1;
}

/**
 * Release resources prior to shutting down
 *****************************************************************************/

static void scard_release_resources()
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
