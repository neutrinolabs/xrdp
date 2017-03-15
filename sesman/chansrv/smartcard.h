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

#ifndef _SMARTCARD_C
#define _SMARTCARD_C

#include "parse.h"
#include "irp.h"
#include "trans.h"

#define SCARD_SHARE_EXCLUSIVE       0x00000001
#define SCARD_SHARE_SHARED          0x00000002
#define SCARD_SHARE_DIRECT          0x00000003

/* see [MS-RDPESC] 2.2.5 protocol identifier - Table A */
#define SCARD_PROTOCOL_UNDEFINED    0x00000000
#define SCARD_PROTOCOL_T0           0x00000001
#define SCARD_PROTOCOL_T1           0x00000002
#define SCARD_PROTOCOL_Tx           0x00000003
#define SCARD_PROTOCOL_RAW          0x00010000

/* see [MS-RDPESC] 2.2.5 protocol identifier - Table B */
#define SCARD_PROTOCOL_DEFAULT      0x80000000
#define SCARD_PROTOCOL_OPTIMAL      0x00000000

/* initialization type */
#define SCARD_LEAVE_CARD            0x00000000 /* do not do anything      */
#define SCARD_RESET_CARD            0x00000001 /* reset smart card        */
#define SCARD_UNPOWER_CARD          0x00000002 /* turn off and reset card */

struct xrdp_scard_io_request
{
    tui32 dwProtocol;
    tui32 cbPciLength;
    int extra_bytes;
    char *extra_data;
};

typedef struct reader_state
{
    char   reader_name[128];
    tui32  current_state;
    tui32  event_state;
    tui32  atr_len; /* number of bytes in atr[] */
    tui8   atr[36];

    /*
     * share mode flag, can be one of:
     *  SCARD_SHARE_EXCLUSIVE  app not willing to share smartcard with other apps
     *  SCARD_SHARE_SHARED     app willing to share smartcard with other apps
     *  SCARD_SHARE_DIRECT     app demands direct control of smart card, hence
     *                         it is not available to other readers
     */
    tui32  dwShareMode;

    /*
     * This field MUST have a value from Table A which is logically
     * OR'ed with a value from Table B.
     */
    tui32  dwPreferredProtocols;

    /*
     * initialization type, must be one of the initialization type
     * defined above
     */
    tui32  init_type;

    /* required by scard_send_transmit(), scard_send_control() */
    tui32 map0;
    tui32 map1;
    tui32 map2;
    tui32 map3;
    tui32 map4;
    tui32 map5;
    tui32 map6;

    tui32 dwProtocol;
    tui32 cbPciLength;
    tui32 cbSendLength;
    tui32 cbRecvLength;
    tui32 dwControlCode;
    tui32 cbOutBufferSize;
    tui32 dwAttribId;
    tui32 dwAttrLen;

} READER_STATE;

void scard_device_announce(tui32 device_id);
int  scard_get_wait_objs(tbus *objs, int *count, int *timeout);
int  scard_check_wait_objs(void);
int  scard_init(void);
int  scard_deinit(void);
int  scard_send_establish_context(void *user_data, int scope);
int  scard_send_release_context(void *user_data,
                                       char *context, int context_bytes);
int  scard_send_is_valid_context(void *user_data,
                                        char *context, int context_bytes);
int  scard_send_list_readers(void *user_data,
                                    char *context, int context_bytes,
                                    char *groups, int cchReaders, int wide);

int  scard_send_get_status_change(void *user_data,
                                         char *context, int context_bytes,
                                         int wide, tui32 timeout,
                                         tui32 num_readers, READER_STATE* rsa);

int  scard_send_connect(void *user_data,
                               char *context, int context_bytes, int wide,
                               READER_STATE* rs);

int  scard_send_reconnect(void *user_data,
                                 char *context, int context_bytes,
                                 char *card, int card_bytes,
                                 READER_STATE* rs);

int  scard_send_begin_transaction(void *user_data,
                                         char *context, int context_bytes,
                                         char *card, int card_bytes);
int  scard_send_end_transaction(void *user_data,
                                       char *context, int context_bytes,
                                       char *card, int card_bytes,
                                       tui32 dwDisposition);
int  scard_send_status(void *user_data, int wide,
                              char *context, int context_bytes,
                              char *card, int card_bytes,
                              int cchReaderLen, int cbAtrLen);
int  scard_send_disconnect(void *user_data,
                                  char *context, int context_bytes,
                                  char *card, int card_bytes,
                                  int dwDisposition);

int  scard_send_transmit(void *user_data,
                                char *context, int context_bytes,
                                char *card, int card_bytes,
                                char *send_data, int send_bytes, int recv_bytes,
                                struct xrdp_scard_io_request *send_ior,
                                struct xrdp_scard_io_request *recv_ior);

int  scard_send_control(void *user_data,
                               char *context, int context_bytes,
                               char *card, int card_bytes,
                               char *send_data, int send_bytes,
                               int recv_bytes, int control_code);

int  scard_send_cancel(void *user_data,
                              char *context, int context_bytes);

int  scard_send_get_attrib(void *user_data, char *card, int card_bytes,
                                  READER_STATE* rs);

/*
 * Notes:
 *      SCardTransmit - partially done
 *      SCardControl - partially done
 *      SCardListReaderGroups - not supported
 *      SCardSetAttrib - not supported
 */
#endif /* end #ifndef _SMARTCARD_C */
