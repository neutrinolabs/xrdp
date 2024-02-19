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
 * types
 */

#ifndef _XRDP_TYPES_H_
#define _XRDP_TYPES_H_

#define DEFAULT_STRING_LEN 255
#define LOG_WINDOW_CHAR_PER_LINE 60

#include "xrdp_rail.h"
#include "xrdp_constants.h"
#include "fifo.h"
#include "guid.h"
#include "xrdp_client_info.h"

#define MAX_NR_CHANNELS 16
#define MAX_CHANNEL_NAME 16

/* Code values used in 'xrdp_mm->code=' settings */
#define XVNC_SESSION_CODE 0
#define XORG_SESSION_CODE 20

/* To check whether touch events has been implemented on session type 'mm' */
#define XRDP_MM_IMPLEMENTS_TOUCH(mm) ((mm)->code != XVNC_SESSION_CODE)

struct source_info;
struct list16;

/* lib */
struct xrdp_mod
{
    int size; /* size of this struct */
    int version; /* internal version */
    /* client functions */
    int (*mod_start)(struct xrdp_mod *v, int w, int h, int bpp);
    int (*mod_connect)(struct xrdp_mod *v);
    int (*mod_event)(struct xrdp_mod *v, int msg, long param1, long param2,
                     long param3, long param4);
    int (*mod_signal)(struct xrdp_mod *v);
    int (*mod_end)(struct xrdp_mod *v);
    int (*mod_set_param)(struct xrdp_mod *v, const char *name, const char *value);
    int (*mod_session_change)(struct xrdp_mod *v, int, int);
    int (*mod_get_wait_objs)(struct xrdp_mod *v, tbus *read_objs, int *rcount,
                             tbus *write_objs, int *wcount, int *timeout);
    int (*mod_check_wait_objs)(struct xrdp_mod *v);
    int (*mod_frame_ack)(struct xrdp_mod *v, int flags, int frame_id);
    int (*mod_suppress_output)(struct xrdp_mod *v, int suppress,
                               int left, int top, int right, int bottom);
    int (*mod_server_monitor_resize)(struct xrdp_mod *v,
                                     int width, int height,
                                     int num_monitors,
                                     const struct monitor_info *monitors,
                                     int *in_progress);
    int (*mod_server_monitor_full_invalidate)(struct xrdp_mod *v,
            int width, int height);
    int (*mod_server_version_message)(struct xrdp_mod *v);
    tintptr mod_dumby[100 - 14]; /* align, 100 minus the number of mod
                                  functions above */
    /* server functions */
    int (*server_begin_update)(struct xrdp_mod *v);
    int (*server_end_update)(struct xrdp_mod *v);
    int (*server_fill_rect)(struct xrdp_mod *v, int x, int y, int cx, int cy);
    int (*server_screen_blt)(struct xrdp_mod *v, int x, int y, int cx, int cy,
                             int srcx, int srcy);
    int (*server_paint_rect)(struct xrdp_mod *v, int x, int y, int cx, int cy,
                             char *data, int width, int height,
                             int srcx, int srcy);
    int (*server_set_pointer)(struct xrdp_mod *v, int x, int y,
                              char *data, char *mask);
    int (*server_palette)(struct xrdp_mod *v, int *palette);
    int (*server_msg)(struct xrdp_mod *v, const char *msg, int code);
    /* This one can be assigned directly into the is_term member of
     * a struct trans */
    int (*server_is_term)(void);
    int (*server_set_clip)(struct xrdp_mod *v, int x, int y, int cx, int cy);
    int (*server_reset_clip)(struct xrdp_mod *v);
    int (*server_set_fgcolor)(struct xrdp_mod *v, int fgcolor);
    int (*server_set_bgcolor)(struct xrdp_mod *v, int bgcolor);
    int (*server_set_opcode)(struct xrdp_mod *v, int opcode);
    int (*server_set_mixmode)(struct xrdp_mod *v, int mixmode);
    int (*server_set_brush)(struct xrdp_mod *v, int x_origin, int y_origin,
                            int style, char *pattern);
    int (*server_set_pen)(struct xrdp_mod *v, int style,
                          int width);
    int (*server_draw_line)(struct xrdp_mod *v, int x1, int y1, int x2, int y2);
    int (*server_add_char)(struct xrdp_mod *v, int font, int character,
                           int offset, int baseline,
                           int width, int height, char *data);
    int (*server_draw_text)(struct xrdp_mod *v, int font,
                            int flags, int mixmode, int clip_left, int clip_top,
                            int clip_right, int clip_bottom,
                            int box_left, int box_top,
                            int box_right, int box_bottom,
                            int x, int y, char *data, int data_len);
    int (*client_monitor_resize)(struct xrdp_mod *v, int width, int height,
                                 int num_monitors,
                                 const struct monitor_info *monitors);
    int (*server_monitor_resize_done)(struct xrdp_mod *v);
    int (*server_get_channel_count)(struct xrdp_mod *v);
    int (*server_query_channel)(struct xrdp_mod *v, int index,
                                char *channel_name,
                                int *channel_flags);
    int (*server_get_channel_id)(struct xrdp_mod *v, const char *name);
    int (*server_send_to_channel)(struct xrdp_mod *v, int channel_id,
                                  char *data, int data_len,
                                  int total_data_len, int flags);
    int (*server_bell_trigger)(struct xrdp_mod *v);
    int (*server_chansrv_in_use)(struct xrdp_mod *v);
    /* off screen bitmaps */
    int (*server_create_os_surface)(struct xrdp_mod *v, int rdpindex,
                                    int width, int height);
    int (*server_switch_os_surface)(struct xrdp_mod *v, int rdpindex);
    int (*server_delete_os_surface)(struct xrdp_mod *v, int rdpindex);
    int (*server_paint_rect_os)(struct xrdp_mod *mod, int x, int y,
                                int cx, int cy,
                                int rdpindex, int srcx, int srcy);
    int (*server_set_hints)(struct xrdp_mod *mod, int hints, int mask);
    /* rail */
    int (*server_window_new_update)(struct xrdp_mod *mod, int window_id,
                                    struct rail_window_state_order *window_state,
                                    int flags);
    int (*server_window_delete)(struct xrdp_mod *mod, int window_id);
    int (*server_window_icon)(struct xrdp_mod *mod,
                              int window_id, int cache_entry, int cache_id,
                              struct rail_icon_info *icon_info,
                              int flags);
    int (*server_window_cached_icon)(struct xrdp_mod *mod,
                                     int window_id, int cache_entry,
                                     int cache_id, int flags);
    int (*server_notify_new_update)(struct xrdp_mod *mod,
                                    int window_id, int notify_id,
                                    struct rail_notify_state_order *notify_state,
                                    int flags);
    int (*server_notify_delete)(struct xrdp_mod *mod, int window_id,
                                int notify_id);
    int (*server_monitored_desktop)(struct xrdp_mod *mod,
                                    struct rail_monitored_desktop_order *mdo,
                                    int flags);
    int (*server_set_pointer_ex)(struct xrdp_mod *v, int x, int y, char *data,
                                 char *mask, int bpp);
    int (*server_add_char_alpha)(struct xrdp_mod *mod, int font, int character,
                                 int offset, int baseline,
                                 int width, int height, char *data);

    int (*server_create_os_surface_bpp)(struct xrdp_mod *v, int rdpindex,
                                        int width, int height, int bpp);
    int (*server_paint_rect_bpp)(struct xrdp_mod *v, int x, int y, int cx, int cy,
                                 char *data, int width, int height,
                                 int srcx, int srcy, int bpp);
    int (*server_composite)(struct xrdp_mod *v, int srcidx, int srcformat,
                            int srcwidth, int srcrepeat, int *srctransform,
                            int mskflags, int mskidx, int mskformat,
                            int mskwidth, int mskrepeat, int op,
                            int srcx, int srcy, int mskx, int msky,
                            int dstx, int dsty, int width, int height,
                            int dstformat);
    int (*server_paint_rects)(struct xrdp_mod *v,
                              int num_drects, short *drects,
                              int num_crects, short *crects,
                              char *data, int width, int height,
                              int flags, int frame_id);
    int (*server_session_info)(struct xrdp_mod *v, const char *data,
                               int data_bytes);
    int (*server_set_pointer_large)(struct xrdp_mod *v, int x, int y,
                                    char *data, char *mask, int bpp,
                                    int width, int height);
    int (*server_paint_rects_ex)(struct xrdp_mod *v,
                                 int num_drects, short *drects,
                                 int num_crects, short *crects,
                                 char *data, int left, int top,
                                 int width, int height,
                                 int flags, int frame_id,
                                 void *shmem_ptr, int shmem_bytes);
    int (*server_egfx_cmd)(struct xrdp_mod *v,
                           char *cmd, int cmd_bytes,
                           char *data, int data_bytes);
    tintptr server_dumby[100 - 50]; /* align, 100 minus the number of server
                                     functions above */
    /* common */
    tintptr handle; /* pointer to self as int */
    tintptr wm; /* struct xrdp_wm* */
    tintptr painter;
    struct source_info *si;
};

/**
 * Transform to apply to loaded images
 */
enum xrdp_bitmap_load_transform
{
    XBLT_NONE = 0,
    XBLT_SCALE,
    XBLT_ZOOM
};

/* header for bmp file */
struct xrdp_bmp_header
{
    int size;
    int image_width;
    int image_height;
    short planes;
    short bit_count;
    int compression;
    int image_size;
    int x_pels_per_meter;
    int y_pels_per_meter;
    int clr_used;
    int clr_important;
};

struct xrdp_palette_item
{
    int stamp;
    int palette[256];
};

struct xrdp_bitmap_item
{
    int stamp;
    int lru_index;
    struct xrdp_bitmap *bitmap;
};

struct xrdp_lru_item
{
    int next;
    int prev;
};

struct xrdp_os_bitmap_item
{
    int id;
    struct xrdp_bitmap *bitmap;
};

struct xrdp_char_item
{
    int stamp;
    struct xrdp_font_char font_item;
};

struct xrdp_pointer_item
{
    int stamp;
    int x; /* hotspot */
    int y;
    int pad0;
    char data[96 * 96 * 4];
    char mask[96 * 96 / 8];
    int bpp;
    int width;
    int height;
    int pad1;
};

struct xrdp_brush_item
{
    int stamp;
    /* expand this to a structure to handle more complicated brushes
       for now it's 8x8 1bpp brushes only */
    char pattern[8];
};

/* moved to xrdp_constants.h
#define XRDP_BITMAP_CACHE_ENTRIES 2048 */

/* difference caches */
struct xrdp_cache
{
    struct xrdp_wm *wm; /* owner */
    struct xrdp_session *session;
    /* palette */
    int palette_stamp;
    struct xrdp_palette_item palette_items[6];
    /* bitmap */
    int bitmap_stamp;
    struct xrdp_bitmap_item bitmap_items[XRDP_MAX_BITMAP_CACHE_ID]
        [XRDP_MAX_BITMAP_CACHE_IDX];

    /* lru optimize */
    struct xrdp_lru_item bitmap_lrus[XRDP_MAX_BITMAP_CACHE_ID]
        [XRDP_MAX_BITMAP_CACHE_IDX];
    int lru_head[XRDP_MAX_BITMAP_CACHE_ID];
    int lru_tail[XRDP_MAX_BITMAP_CACHE_ID];
    int lru_reset[XRDP_MAX_BITMAP_CACHE_ID];

    /* crc optimize */
    struct list16 crc16[XRDP_MAX_BITMAP_CACHE_ID][64 * 1024];

    int use_bitmap_comp;
    int cache1_entries;
    int cache1_size;
    int cache2_entries;
    int cache2_size;
    int cache3_entries;
    int cache3_size;
    int bitmap_cache_persist_enable;
    int bitmap_cache_version;
    /* font */
    int char_stamp;
    struct xrdp_char_item char_items[12][256];
    /* pointer */
    int pointer_stamp;
    struct xrdp_pointer_item pointer_items[32];
    int pointer_cache_entries;
    int brush_stamp;
    struct xrdp_brush_item brush_items[64];
    struct xrdp_os_bitmap_item os_bitmap_items[2000];
    struct list *xrdp_os_del_list;
};

/* defined later */
struct xrdp_enc_data;

/**
 * Stages we go through connecting to the session
 */
enum mm_connect_state
{
    MMCS_CONNECT_TO_SESMAN,
    MMCS_GATEWAY_LOGIN,
    MMCS_SESSION_LOGIN,
    MMCS_CREATE_SESSION,
    MMCS_CONNECT_TO_SESSION,
    MMCS_CONNECT_TO_CHANSRV,
    MMCS_DONE
};

enum display_resize_state
{
    WMRZ_ENCODER_DELETE = 0,
    WMRZ_EGFX_DELETE_SURFACE,
    WMRZ_EGFX_CONN_CLOSE,
    WMRZ_EGFX_CONN_CLOSING,
    WMRZ_EGFX_CONN_CLOSED,
    WRMZ_EGFX_DELETE,
    WMRZ_SERVER_MONITOR_RESIZE,
    WMRZ_SERVER_MONITOR_MESSAGE_PROCESSING,
    WMRZ_SERVER_MONITOR_MESSAGE_PROCESSED,
    WMRZ_XRDP_CORE_RESET,
    WMRZ_XRDP_CORE_RESET_PROCESSING,
    WMRZ_XRDP_CORE_RESET_PROCESSED,
    WMRZ_EGFX_INITIALIZE,
    WMRZ_EGFX_INITALIZING,
    WMRZ_EGFX_INITIALIZED,
    WMRZ_ENCODER_CREATE,
    WMRZ_SERVER_INVALIDATE,
    WMRZ_COMPLETE,
    WMRZ_ERROR
};

#define XRDP_DISPLAY_RESIZE_STATE_TO_STR(status) \
    ((status) == WMRZ_ENCODER_DELETE ? "WMRZ_ENCODER_DELETE" : \
     (status) == WMRZ_EGFX_DELETE_SURFACE ? "WMRZ_EGFX_DELETE_SURFACE" : \
     (status) == WMRZ_EGFX_CONN_CLOSE ? "WMRZ_EGFX_CONN_CLOSE" : \
     (status) == WMRZ_EGFX_CONN_CLOSING ? "WMRZ_EGFX_CONN_CLOSING" : \
     (status) == WMRZ_EGFX_CONN_CLOSED ? "WMRZ_EGFX_CONN_CLOSED" : \
     (status) == WRMZ_EGFX_DELETE ? "WMRZ_EGFX_DELETE" : \
     (status) == WMRZ_SERVER_MONITOR_RESIZE ? "WMRZ_SERVER_MONITOR_RESIZE" : \
     (status) == WMRZ_SERVER_MONITOR_MESSAGE_PROCESSING ? \
     "WMRZ_SERVER_MONITOR_MESSAGE_PROCESSING" : \
     (status) == WMRZ_SERVER_MONITOR_MESSAGE_PROCESSED ? \
     "WMRZ_SERVER_MONITOR_MESSAGE_PROCESSED" : \
     (status) == WMRZ_XRDP_CORE_RESET ? "WMRZ_XRDP_CORE_RESET" : \
     (status) == WMRZ_XRDP_CORE_RESET_PROCESSING ? \
     "WMRZ_XRDP_CORE_RESET_PROCESSING" : \
     (status) == WMRZ_XRDP_CORE_RESET_PROCESSED ? \
     "WMRZ_XRDP_CORE_RESET_PROCESSED" : \
     (status) == WMRZ_EGFX_INITIALIZE ? "WMRZ_EGFX_INITIALIZE" : \
     (status) == WMRZ_EGFX_INITALIZING ? "WMRZ_EGFX_INITALIZING" : \
     (status) == WMRZ_EGFX_INITIALIZED ? "WMRZ_EGFX_INITIALIZED" : \
     (status) == WMRZ_ENCODER_CREATE ? "WMRZ_ENCODER_CREATE" : \
     (status) == WMRZ_SERVER_INVALIDATE ? "WMRZ_SERVER_INVALIDATE" : \
     (status) == WMRZ_COMPLETE ? "WMRZ_COMPLETE" : \
     (status) == WMRZ_ERROR ? "WMRZ_ERROR" : \
     "unknown" \
    )

enum xrdp_egfx_flags
{
    XRDP_EGFX_NONE = 0,
    XRDP_EGFX_H264 = 1,
    XRDP_EGFX_RFX_PRO = 2
};

struct xrdp_mm
{
    struct xrdp_wm *wm; /* owner */
    enum mm_connect_state connect_state; /* State of connection */
    int mmcs_expecting_msg; /* Connect state machine is expecting
                               a message from sesman */
    /* Other processes we connect to */
    int use_sesman; /* true if this is a sesman session */
    int use_gw_login; /* True if we're to login using  a gateway */
    int use_chansrv; /* true if chansrvport is set in xrdp.ini or using sesman */
    struct trans *sesman_trans; /* connection to sesman */
    struct trans *chan_trans; /* connection to chansrv */

    /* We can't delete transports while we're in a callback for that
     * transport, as this causes trans.c to reference undefined memory.
     * These flags mark transports as needing to be deleted when
     * we are definitely not in a transport callback */
    int delete_sesman_trans;

    struct list *login_names;
    struct list *login_values;
    /* mod vars */
    long mod_handle; /* returned from g_load_library */
    struct xrdp_mod *(*mod_init)(void);
    int (*mod_exit)(struct xrdp_mod *);
    struct xrdp_mod *mod; /* module interface */
    int display; /* 10 for :10.0, 11 for :11.0, etc */
    int uid; /* UID for a successful login, -1 otherwise */
    struct guid guid; /* GUID for the session, or all zeros  */
    int code; /* 0=Xvnc session, 20=xorg driver mode */
    struct xrdp_encoder *encoder;
    int cs2xr_cid_map[256];
    int xr2cr_cid_map[256];
    int dynamic_monitor_chanid;
    struct xrdp_egfx *egfx;
    int egfx_up;
    enum xrdp_egfx_flags egfx_flags;
    int gfx_delay_autologin;
    int mod_uses_wm_screen_for_gfx;
    /* Resize on-the-fly control */
    struct display_control_monitor_layout_data *resize_data;
    struct list *resize_queue;
    tbus resize_ready;
};

struct xrdp_key_info
{
    int sym;
    char32_t chr;
};

struct xrdp_keymap
{
    struct xrdp_key_info keys_noshift[256];
    struct xrdp_key_info keys_shift[256];
    struct xrdp_key_info keys_altgr[256];
    struct xrdp_key_info keys_shiftaltgr[256];
    struct xrdp_key_info keys_capslock[256];
    struct xrdp_key_info keys_capslockaltgr[256];
    struct xrdp_key_info keys_shiftcapslock[256];
    struct xrdp_key_info keys_shiftcapslockaltgr[256];
};

/* the window manager */

/***
 * Window manager login mode states
 *
 * Use with xrdp_wm_set_login_state()
 */
enum wm_login_state
{
    /**
     * Place the window manager in this state to reset it
     */
    WMLS_RESET = 0,
    /**
     * In this state, the window manager is waiting for the user to fill
     * in the login box
     */
    WMLS_USER_PROMPT,
    /**
     * Place the window manager in this state to request xrdp connects to
     * the X server, sesman, chansrv etc
     */
    WMLS_START_CONNECT,
    /**
     * In this state, the window manager is making required connections
     */
    WMLS_CONNECT_IN_PROGRESS,
    /**
     * Place the window manager in this state to request it finishes.
     */
    WMLS_CLEANUP,
    /**
     * In this state, the window manager is inactive
     */
    WMLS_INACTIVE
};

struct xrdp_wm
{
    struct xrdp_process *pro_layer; /* owner */
    struct xrdp_bitmap *screen;
    struct xrdp_session *session;
    struct xrdp_painter *painter;
    struct xrdp_cache *cache;
    int palette[256];
    struct xrdp_bitmap *login_window;
    /* generic colors */
    int black;
    int grey;
    int dark_grey;
    int blue;
    int dark_blue;
    int white;
    int red;
    int green;
    int background;
    /* dragging info */
    int dragging;
    int draggingx;
    int draggingy;
    int draggingcx;
    int draggingcy;
    int draggingdx;
    int draggingdy;
    int draggingorgx;
    int draggingorgy;
    int draggingxorstate;
    struct xrdp_bitmap *dragging_window;
    /* the down(clicked) button */
    struct xrdp_bitmap *button_down;
    /* popup for combo box */
    struct xrdp_bitmap *popup_wnd;
    /* focused window */
    struct xrdp_bitmap *focused_window;
    /* pointer */
    int current_pointer;
    int mouse_x;
    int mouse_y;
    /* keyboard info */
    int keys[256]; /* key states 0 up 1 down*/
    int caps_lock;
    int scroll_lock;
    int num_lock;
    /* client info */
    struct xrdp_client_info *client_info;
    /* session log */
    struct list *log;
    struct xrdp_bitmap *log_wnd;
    enum wm_login_state login_state;
    tbus login_state_event;
    struct xrdp_mm *mm;
    struct xrdp_font *default_font;
    struct xrdp_keymap keymap;
    int hide_log_window;
    int fatal_error_in_log_window;
    struct xrdp_bitmap *target_surface; /* either screen or os surface */
    int current_surface_index;
    int hints;
    char pamerrortxt[256];

    /* configuration derived from xrdp.ini */
    struct xrdp_config *xrdp_config;

    struct xrdp_region *screen_dirty_region;
    int last_screen_draw_time;
};

/* rdp process */
struct xrdp_process
{
    int status;
    struct trans *server_trans; /* in tcp server mode */
    tbus self_term_event;
    struct xrdp_listen *lis_layer; /* owner */
    struct xrdp_session *session;
    /* create these when up and running */
    struct xrdp_wm *wm;
    //int app_sck;
    tbus done_event;
    int session_id;
};

/* rdp listener */
struct xrdp_listen
{
    int status;
    struct list *trans_list; /* list of struct trans* */
    struct list *process_list;
    struct list *fork_list;
    tbus pro_done_event;
    struct xrdp_startup_params *startup_params;
};

/* region */
struct xrdp_region
{
    struct xrdp_wm *wm; /* owner */
    struct pixman_region16 *reg;
};

/* painter */
struct xrdp_painter
{
    int rop;
    struct xrdp_rect *use_clip; /* nil if not using clip */
    struct xrdp_rect clip;
    int clip_children;
    int bg_color;
    int fg_color;
    int mix_mode;
    struct xrdp_brush brush;
    struct xrdp_pen pen;
    struct xrdp_session *session;
    struct xrdp_wm *wm; /* owner */
    struct xrdp_font *font;
    void *painter;
    struct xrdp_region *dirty_region;
    int begin_end_level;
};

/* window or bitmap */
struct xrdp_bitmap
{
    /* 0 = bitmap 1 = window 2 = screen 3 = button 4 = image 5 = edit
       6 = label 7 = combo 8 = special */
    int type;
    int width;
    int height;
    struct xrdp_wm *wm;
    /* msg 1 = click 2 = mouse move 3 = paint 100 = modal result */
    /* see messages in constants.h */
    int (*notify)(struct xrdp_bitmap *wnd, struct xrdp_bitmap *sender,
                  int msg, long param1, long param2);
    /* for bitmap */
    int bpp;
    int line_size; /* in bytes */
    int do_not_free_data;
    char *data;
    /* for all but bitmap */
    int left;
    int top;
    int pointer;
    int bg_color;
    int tab_stop;
    int id;
    char *caption1;
    /* for window or screen */
    struct xrdp_bitmap *modal_dialog;
    struct xrdp_bitmap *focused_control;
    struct xrdp_bitmap *owner; /* window that created us */
    struct xrdp_bitmap *parent; /* window contained in */
    /* for modal dialog */
    struct xrdp_bitmap *default_button; /* button when enter is pressed */
    struct xrdp_bitmap *esc_button; /* button when esc is pressed */
    /* list of child windows */
    struct list *child_list;
    /* for edit */
    int edit_pos;
    char32_t password_char;
    /* for button or combo */
    int state; /* for button 0 = normal 1 = down */
    /* for combo */
    struct list *string_list;
    struct list *data_list;
    /* for combo or popup */
    int item_index;
    /* for popup */
    struct xrdp_bitmap *popped_from;
    int item_height;
    /* crc */
    int crc32;
    int crc16;
};

#define MAX_FONT_CHARS 0x4e00
#define DEFAULT_FONT_NAME "sans-10.fv1"
#define DEFAULT_FONT_PIXEL_SIZE 16
#define DEFAULT_FV1_SELECT "130:sans-18.fv1,0:" DEFAULT_FONT_NAME

#define DEFAULT_BUTTON_MARGIN_H 12
#define DEFAULT_BUTTON_MARGIN_W 12
#define DEFAULT_COMBO_MARGIN_H 6
#define DEFAULT_EDIT_MARGIN_H  6
#define DEFAULT_WND_LOGIN_W   425
#define DEFAULT_WND_LOGIN_H   475
#define DEFAULT_WND_HELP_W    340
#define DEFAULT_WND_HELP_H    300
#define DEFAULT_WND_LOG_W     400
#define DEFAULT_WND_LOG_H     400
#define DEFAULT_WND_SPECIAL_H 100

/* font */
struct xrdp_font
{
    struct xrdp_wm *wm;
    // Font characters, accessed by Unicode codepoint. The first 32
    // entries are unused.
    struct xrdp_font_char chars[MAX_FONT_CHARS];
    unsigned int char_count; // # elements in above array
    struct xrdp_font_char *default_char; // Pointer into above array
    char name[32];
    int size;
    /** Body height in pixels */
    int body_height;
    int style;
};

/* module */
struct xrdp_mod_data
{
    struct list *names;
    struct list *values;
};

struct xrdp_startup_params
{
    /* xrdp_ini is not malloc'd and has at least the same lifetime as main() */
    const char *xrdp_ini;
    char port[1024];
    int kill;
    int no_daemon;
    int help;
    int version;
    int fork;
    int dump_config;
    int license;
    int tcp_send_buffer_bytes;
    int tcp_recv_buffer_bytes;
    int tcp_nodelay;
    int tcp_keepalive;
    int use_vsock;
};

/*
 * For storing xrdp.ini (and other) configuration settings
 */

struct xrdp_ls_dimensions
{
    int  width;               /* window width */
    int  height;              /* window height */
    int  logo_width;          /* logo width (optional) */
    int  logo_height;          /* logo height (optional) */
    int  logo_x_pos;          /* logo x co-ordinate */
    int  logo_y_pos;          /* logo y co-ordinate */
    int  label_x_pos;         /* x pos of labels */
    int  label_width;         /* width of labels */
    int  input_x_pos;         /* x pos of text and combo boxes */
    int  input_width;         /* width of input and combo boxes */
    int  input_y_pos;         /* y pos for for first label and combo box */
    int  btn_ok_x_pos;        /* x pos for OK button */
    int  btn_ok_y_pos;        /* y pos for OK button */
    int  btn_ok_width;        /* width of OK button */
    int  btn_ok_height;       /* height of OK button */
    int  btn_cancel_x_pos;    /* x pos for Cancel button */
    int  btn_cancel_y_pos;    /* y pos for Cancel button */
    int  btn_cancel_width;    /* width of Cancel button */
    int  btn_cancel_height;   /* height of Cancel button */
    int default_btn_height;   /* Default button height (e.g. OK on login box) */
    int log_wnd_width;        /* Width of log window */
    int log_wnd_height;       /* Height of log window */
    int edit_height;          /* Height of an edit box */
    int combo_height;         /* Height of a combo box */
    int help_wnd_width;        /* Width of login help window */
    int help_wnd_height;       /* Height of login help window */
};

struct xrdp_cfg_globals
{
    int  ini_version;            /* xrdp.ini file version number */
    int  use_bitmap_cache;
    int  use_bitmap_compression;
    int  port;
    int  crypt_level;            /* low=1, medium=2, high=3 */
    int  allow_channels;
    int  max_bpp;
    int  fork;
    int  tcp_nodelay;
    int  tcp_keepalive;
    int  tcp_send_buffer_bytes;
    int  tcp_recv_buffer_bytes;
    char autorun[256];
    int  hidelogwindow;
    int  require_credentials;
    int  bulk_compression;
    int  new_cursors;
    int  nego_sec_layer;
    int  allow_multimon;
    int  enable_token_login;

    /* colors */

    int  grey;
    int  black;
    int  dark_grey;
    int  blue;
    int  dark_blue;
    int  white;
    int  red;
    int  green;
    int  background;

    /* login screen */
    unsigned int  default_dpi;   /* Default DPI to use if nothing from client */
    char fv1_select[256];        /* Selection string for fv1 font */
    int  ls_top_window_bg_color; /* top level window background color */
    int  ls_bg_color;            /* background color */
    char ls_background_image[256];  /* background image file name */
    /* transform to apply to background image */
    enum xrdp_bitmap_load_transform ls_background_transform;
    char ls_logo_filename[256];  /* logo filename */
    /* transform to apply to logo */
    enum xrdp_bitmap_load_transform ls_logo_transform;
    char ls_title[256];          /* loginscreen window title */
    /* Login screen dimensions, unscaled (from config) */
    struct xrdp_ls_dimensions ls_unscaled;
    /* Login screen dimensions, scaled (after font is loaded) */
    struct xrdp_ls_dimensions ls_scaled;
};

struct xrdp_cfg_logging
{

};

struct xrdp_cfg_channels
{

};

struct xrdp_config
{
    struct xrdp_cfg_globals   cfg_globals;
    struct xrdp_cfg_logging   cfg_logging;
    struct xrdp_cfg_channels  cfg_channels;
};

#endif
