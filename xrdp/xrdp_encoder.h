
#ifndef _XRDP_ENCODER_H
#define _XRDP_ENCODER_H

#include "arch.h"
#include "fifo.h"
#include "xrdp_client_info.h"

#define ENC_IS_BIT_SET(_flags, _bit) (((_flags) & (1 << (_bit))) != 0)
#define ENC_SET_BIT(_flags, _bit) do { _flags |= (1 << (_bit)); } while (0)
#define ENC_CLR_BIT(_flags, _bit) do { _flags &= ~(1 << (_bit)); } while (0)
#define ENC_SET_BITS(_flags, _mask, _bits) \
    do { _flags &= ~(_mask); _flags |= (_bits) & (_mask); } while (0)

struct xrdp_enc_data;

/* for codec mode operations */
struct xrdp_encoder
{
    struct xrdp_mm *mm;
    int in_codec_mode;
    int codec_id;
    int codec_quality;
    int max_compressed_bytes;
    tbus xrdp_encoder_event_to_proc;
    tbus xrdp_encoder_event_processed;
    tbus xrdp_encoder_term;
    struct fifo *fifo_to_proc;
    struct fifo *fifo_processed;
    tbus mutex;
    int (*process_enc)(struct xrdp_encoder *self, struct xrdp_enc_data *enc);
    void *codec_handle_rfx;
    void *codec_handle_jpg;
    void *codec_handle_h264;
    void *codec_handle_prfx_gfx[16];
    void *codec_handle_h264_gfx[16];
    int frame_id_client; /* last frame id received from client */
    int frame_id_server; /* last frame id received from Xorg */
    int frame_id_server_sent;
    int frames_in_flight;
    int gfx;
    int gfx_ack_off;
    const char *quants;
    int num_quants;
    int quant_idx_y;
    int quant_idx_u;
    int quant_idx_v;
};

/* cmd_id = 0 */
struct xrdp_enc_surface_command
{
    struct xrdp_mod *mod;
    int num_drects;
    int pad0;
    short *drects;  /* 4 * num_drects */
    int num_crects;
    int pad1;
    short *crects;  /* 4 * num_crects */
    char *data;
    int left;
    int top;
    int width;
    int height;
    int flags;
    int frame_id;
};

struct xrdp_enc_gfx_cmd
{
    char *cmd;
    char *data;
    int cmd_bytes;
    int data_bytes;
};

typedef struct xrdp_enc_data XRDP_ENC_DATA;

#define ENC_DONE_FLAGS_GFX_BIT      0
#define ENC_DONE_FLAGS_FRAME_ID_BIT 1

/* used when scheduling tasks from xrdp_encoder.c */
struct xrdp_enc_data_done
{
    int comp_bytes;
    int pad_bytes;
    char *comp_pad_data;
    struct xrdp_enc_data *enc;
    int last; /* true is this is last message for enc */
    int continuation; /* true if this isn't the start of a frame */
    int x;
    int y;
    int cx;
    int cy;
    int flags; /* ENC_DONE_FLAGS_* */
    int frame_id;
};

#define ENC_FLAGS_GFX_BIT   0

/* used when scheduling tasks in xrdp_encoder.c */
struct xrdp_enc_data
{
    struct xrdp_mod *mod;
    int flags; /* ENC_FLAGS_* */
    int pad0;
    void *shmem_ptr;
    int shmem_bytes;
    int pad1;
    union _u
    {
        struct xrdp_enc_surface_command sc;
        struct xrdp_enc_gfx_cmd gfx;
    } u;
};

typedef struct xrdp_enc_data_done XRDP_ENC_DATA_DONE;

struct xrdp_encoder *
xrdp_encoder_create(struct xrdp_mm *mm);
void
xrdp_encoder_delete(struct xrdp_encoder *self);
THREAD_RV THREAD_CC
proc_enc_msg(void *arg);

#endif
