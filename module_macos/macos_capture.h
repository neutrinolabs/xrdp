/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) 2026 Neutrinos Software Corporation
 * Some portions Classify(r)
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

/**
 * @file macos_capture.h
 * @brief Native macOS screen capture module for xrdp using ScreenCaptureKit
 *
 * This module provides direct screen capture on macOS without requiring VNC.
 * It uses ScreenCaptureKit (macOS 12.3+) or CGDisplayStream (fallback) for
 * efficient, GPU-accelerated screen capture with damage region detection.
 */

#ifndef MACOS_CAPTURE_H
#define MACOS_CAPTURE_H

#include "xrdp_types.h"

#ifdef __OBJC__
#import <Foundation/Foundation.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <IOSurface/IOSurface.h>
#endif

/* Module state structure */
struct mod_macos
{
    int size;              /* Size of this structure */

    /* Connection info */
    tbus handle;           /* Main handle */
    tbus wm;              /* Window manager handle */
    tbus painter;         /* Painter handle */

    /* Server callbacks */
    int (*server_begin_update)(struct mod_macos* mod);
    int (*server_end_update)(struct mod_macos* mod);
    int (*server_fill_rect)(struct mod_macos* mod, int x, int y, int cx, int cy);
    int (*server_screen_blt)(struct mod_macos* mod, int x, int y, int cx, int cy,
                            int srcx, int srcy);
    int (*server_paint_rect)(struct mod_macos* mod, int x, int y, int cx, int cy,
                            char* data, int width, int height, int srcx, int srcy);
    int (*server_set_pointer)(struct mod_macos* mod, int x, int y,
                             char* data, char* mask);
    int (*server_set_cursor)(struct mod_macos* mod, int x, int y,
                            char* data, char* mask);
    int (*server_palette)(struct mod_macos* mod, int* palette);
    int (*server_msg)(struct mod_macos* mod, char* msg, int code);
    int (*server_is_term)(struct mod_macos* mod);
    int (*server_set_clip)(struct mod_macos* mod, int x, int y, int cx, int cy);
    int (*server_reset_clip)(struct mod_macos* mod);
    int (*server_set_fgcolor)(struct mod_macos* mod, int fgcolor);
    int (*server_set_bgcolor)(struct mod_macos* mod, int bgcolor);
    int (*server_set_opcode)(struct mod_macos* mod, int opcode);
    int (*server_set_mixmode)(struct mod_macos* mod, int mixmode);
    int (*server_set_brush)(struct mod_macos* mod, int x_origin, int y_origin,
                           int style, char* pattern);
    int (*server_set_pen)(struct mod_macos* mod, int style, int width);
    int (*server_draw_line)(struct mod_macos* mod, int x1, int y1, int x2, int y2);
    int (*server_add_char)(struct mod_macos* mod, int font, int character,
                          int offset, int baseline, int width, int height,
                          char* data);
    int (*server_draw_text)(struct mod_macos* mod, int font, int flags, int mixmode,
                           int clip_left, int clip_top, int clip_right, int clip_bottom,
                           int box_left, int box_top, int box_right, int box_bottom,
                           int x, int y, char* data, int data_len);
    int (*server_reset)(struct mod_macos* mod, int width, int height, int bpp);
    int (*server_query_channel)(struct mod_macos* mod, int index, char* channel_name,
                               int* channel_flags);
    int (*server_get_channel_id)(struct mod_macos* mod, char* name);
    int (*server_send_to_channel)(struct mod_macos* mod, int channel_id, char* data,
                                 int data_len, int total_data_len, int flags);
    int (*server_bell_trigger)(struct mod_macos* mod);
    int (*server_chansrv_in_use)(struct mod_macos* mod);
    tintptr (*server_get_channel_handle)(struct mod_macos* mod);

    /* Client info */
    int width;            /* Screen width */
    int height;           /* Screen height */
    int bpp;              /* Bits per pixel (16, 24, or 32) */

    /* Capture state */
    void* capture_context;   /* Opaque pointer to Objective-C capture object */
    void* frame_buffer;      /* Current frame buffer */
    int frame_buffer_size;   /* Size of frame buffer */

    /* Damage tracking */
    int dirty_x;          /* Dirty region x */
    int dirty_y;          /* Dirty region y */
    int dirty_cx;         /* Dirty region width */
    int dirty_cy;         /* Dirty region height */
    int has_damage;       /* Dirty flag */

    /* Input state */
    int mouse_x;          /* Current mouse X */
    int mouse_y;          /* Current mouse Y */
    int mouse_buttons;    /* Current mouse button state */

    /* Performance tracking */
    tui64 last_frame_time;   /* Last frame timestamp */
    int frame_count;         /* Frame counter */
};

/* Module interface functions */
extern tintptr EXPORT_CC
mod_init(void);

extern int EXPORT_CC
mod_exit(tintptr handle);

extern int EXPORT_CC
mod_start(tintptr handle, int w, int h, int bpp);

extern int EXPORT_CC
mod_connect(tintptr handle);

extern int EXPORT_CC
mod_event(tintptr handle, int msg, tbus param1, tbus param2, tbus param3, tbus param4);

extern int EXPORT_CC
mod_signal(tintptr handle);

extern int EXPORT_CC
mod_end(tintptr handle);

extern int EXPORT_CC
mod_set_param(tintptr handle, char* name, char* value);

extern int EXPORT_CC
mod_get_wait_objs(tintptr handle, tbus* read_objs, int* rcount,
                  tbus* write_objs, int* wcount, int* timeout);

extern int EXPORT_CC
mod_check_wait_objs(tintptr handle);

extern int EXPORT_CC
mod_frame_ack(tintptr handle, int flags, int frame_id);

extern int EXPORT_CC
mod_suppress_output(tintptr handle, int suppress,
                    int left, int top, int right, int bottom);

extern int EXPORT_CC
mod_server_version_message(tintptr handle);

extern int EXPORT_CC
mod_server_monitor_resize(tintptr handle, int width, int height);

/* Internal helper functions */
int macos_capture_init(struct mod_macos* mod);
int macos_capture_start(struct mod_macos* mod);
int macos_capture_stop(struct mod_macos* mod);
int macos_capture_get_frame(struct mod_macos* mod);
int macos_capture_deinit(struct mod_macos* mod);

int macos_input_mouse_event(struct mod_macos* mod, int x, int y, int flags);
int macos_input_keyboard_event(struct mod_macos* mod, int flags, int key);

int macos_convert_pixel_format(char* dest, char* src, int width, int height,
                               int src_format, int dest_bpp);

#endif /* MACOS_CAPTURE_H */
