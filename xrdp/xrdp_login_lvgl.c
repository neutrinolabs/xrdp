/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Idan Freiberg 2026
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
 * Optional LVGL pre-session UI.
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "xrdp.h"
#include "xrdp_login.h"
#include "xrdp_login_lvgl.h"
#include "string_calls.h"

void
xrdp_login_lvgl_prepare_connect(struct xrdp_wm *wm)
{
    if (wm->login_ui != NULL)
    {
        /* Drop queued login pixels before the module can paint its first frame.
         * Keep the widgets available if connecting fails. */
        xrdp_region_delete(wm->screen_dirty_region);
        wm->screen_dirty_region = NULL;
        wm->mm->mod_uses_wm_screen_for_gfx = 0;
    }
}

#ifdef XRDP_LVGL
#include <lvgl.h>
#include <fontconfig/fontconfig.h>
#include <openssl/crypto.h>
#include <pthread.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* Limit backing storage to 64 MiB (32-bit pixels), also on resize. */
#define MAX_UI_PIXELS (16 * 1024 * 1024)
enum ui_action { UI_NONE, UI_SUBMIT, UI_CANCEL, UI_SELECT, UI_HELP, UI_CLOSE_HELP, UI_ACK };

struct xrdp_login_lvgl
{
    lv_display_t *display;
    lv_indev_t *pointer;
    lv_group_t *group;
    lv_font_t *font;
    lv_font_t *heading_font;
    lv_font_t *log_font;
    lv_obj_t *card;
    lv_obj_t *fields;
    lv_obj_t *sessions;
    lv_obj_t *status;
    lv_obj_t *log_title;
    lv_obj_t *log_rows;
    lv_obj_t *help;
    lv_obj_t *submit;
    lv_obj_t **edits; /* indexed by module parameter, not visual position */
    int edit_count;
    struct list *names;
    struct list *modules;
    int selected;
    int mode; /* 0 = login, 1 = connecting, 2 = log acknowledgement */
    unsigned int dpi;
    int primary_x, primary_y, primary_width, primary_height;
    int action;
    int dirty;
    struct xrdp_rect damage;
    struct xrdp_bitmap *frame;
    void *draw_buffer;
    struct xrdp_bitmap *logo;
    struct xrdp_bitmap *background;
    lv_image_dsc_t logo_image;
    lv_image_dsc_t background_image;
    lv_point_t mouse;
    int pressed;
    tbus event;
};

/* ponytail: rendering is serialized per process; use fork=true if parallel
 * login rendering becomes a throughput bottleneck. Never send RDP under lock. */
static pthread_mutex_t ui_mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned int ui_users;
static FcConfig *font_config;
static unsigned int timer_due;

/* Widget changes can resume an idle LVGL timer; discard its old deadline. */
static void
refresh_soon(struct xrdp_login_lvgl *ui)
{
    timer_due = g_get_elapsed_ms();
    g_set_wait_obj(ui->event);
}

static int
px(const struct xrdp_login_lvgl *ui, int value)
{
    return value * ui->dpi / 96;
}

static void
mark_dirty(struct xrdp_login_lvgl *ui, int left, int top, int right, int bottom)
{
    left = MAX(0, left);
    top = MAX(0, top);
    right = MIN(ui->frame->width, right);
    bottom = MIN(ui->frame->height, bottom);
    if (right <= left || bottom <= top)
    {
        return;
    }
    if (ui->dirty)
    {
        left = MIN(left, ui->damage.left);
        top = MIN(top, ui->damage.top);
        right = MAX(right, ui->damage.right);
        bottom = MAX(bottom, ui->damage.bottom);
    }
    ui->damage = (struct xrdp_rect)
    {
        left, top, right, bottom
    };
    ui->dirty = 1;
    g_set_wait_obj(ui->event);
}

static void
flush_pixels(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    struct xrdp_login_lvgl *ui = lv_display_get_user_data(display);
    int left = MAX(0, area->x1);
    int top = MAX(0, area->y1);
    int right = MIN(ui->frame->width, area->x2 + 1);
    int bottom = MIN(ui->frame->height, area->y2 + 1);
    uint32_t stride = lv_draw_buf_width_to_stride(lv_area_get_width(area), LV_COLOR_FORMAT_XRGB8888);
    int x;
    int y;
    for (y = top; y < bottom; ++y)
    {
        const uint32_t *src = (const uint32_t *)(pixels + (y - area->y1) * stride);
        char *dst = ui->frame->data + y * ui->frame->line_size;
        for (x = left; x < right; ++x)
        {
            uint32_t rgb = src[x - area->x1] & 0xffffff;
            if (ui->frame->bpp == 15)
            {
                ((uint16_t *)dst)[x] = COLOR15(rgb >> 16, (rgb >> 8) & 255, rgb & 255);
            }
            else if (ui->frame->bpp == 16)
            {
                ((uint16_t *)dst)[x] = COLOR16(rgb >> 16, (rgb >> 8) & 255, rgb & 255);
            }
            else
            {
                ((uint32_t *)dst)[x] = rgb;
            }
        }
    }
    mark_dirty(ui, left, top, right, bottom);
    lv_display_flush_ready(display);
}

static void
read_pointer(lv_indev_t *device, lv_indev_data_t *data)
{
    struct xrdp_login_lvgl *ui = lv_indev_get_user_data(device);
    data->point = ui->mouse;
    data->state = ui->pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void
request_action(lv_event_t *event)
{
    struct xrdp_login_lvgl *ui = lv_event_get_user_data(event);
    lv_obj_t *obj = lv_event_get_target_obj(event);
    if (ui->action == UI_NONE)
    {
        ui->action = (int)(intptr_t)lv_obj_get_user_data(obj);
        g_set_wait_obj(ui->event);
    }
}

static lv_obj_t *
label(lv_obj_t *parent, const char *text)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_label_set_text(obj, text);
    lv_obj_set_width(obj, LV_PCT(100));
    return obj;
}

static lv_obj_t *
heading(struct xrdp_login_lvgl *ui, lv_obj_t *parent, const char *text)
{
    lv_obj_t *obj = label(parent, text);
    lv_obj_set_style_text_font(obj, ui->heading_font, 0);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, 0);
    return obj;
}

/* Shared by the login card and its help sheet. No blur or animation work
 * is needed when the screen is idle. */
static void
style_sheet(struct xrdp_login_lvgl *ui, lv_obj_t *obj)
{
    lv_obj_set_style_pad_all(obj, px(ui, 32), 0);
    lv_obj_set_style_pad_row(obj, px(ui, 18), 0);
    lv_obj_set_style_anim_duration(obj, 0, 0);
    lv_obj_set_style_radius(obj, px(ui, 36), 0);
    lv_obj_set_style_bg_color(obj, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(obj, 220, 0);
    lv_obj_set_style_border_color(obj, lv_color_white(), 0);
    lv_obj_set_style_border_opa(obj, LV_OPA_80, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_shadow_color(obj, lv_color_hex(0x535d9c), 0);
    lv_obj_set_style_shadow_width(obj, px(ui, 48), 0);
    lv_obj_set_style_shadow_ofs_y(obj, px(ui, 16), 0);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_10, 0);
}

static void
style_control(struct xrdp_login_lvgl *ui, lv_obj_t *obj)
{
    lv_obj_set_style_radius(obj, px(ui, 18), 0);
    lv_obj_set_style_bg_color(obj, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_80, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(0xdfe3ef), 0);
    lv_obj_set_style_pad_all(obj, px(ui, 16), 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(0x007aff), LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(obj, lv_color_hex(0x007aff), LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(obj, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(obj, 2, LV_STATE_FOCUSED);
}

static lv_obj_t *
button(struct xrdp_login_lvgl *ui, lv_obj_t *parent, const char *text, int action)
{
    lv_obj_t *obj = lv_button_create(parent);
    int primary = action == UI_SUBMIT || action == UI_ACK || action == UI_CLOSE_HELP;
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_height(obj, px(ui, 54));
    lv_obj_set_width(obj, primary ? LV_PCT(100) : LV_PCT(47));
    lv_obj_set_style_bg_color(obj, lv_color_hex(primary ? 0x0066d6 : 0xffffff), 0);
    lv_obj_set_style_bg_opa(obj, primary ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
    lv_obj_set_style_text_color(obj, primary ? lv_color_white() : lv_color_hex(0x2465ad), 0);
    lv_obj_set_style_outline_color(obj, lv_color_hex(0x007aff), LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(obj, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(obj, 3, LV_STATE_FOCUSED);
    lv_obj_set_user_data(obj, (void *)(intptr_t)action);
    lv_obj_add_event_cb(obj, request_action, LV_EVENT_CLICKED, ui);
    lv_group_add_obj(ui->group, obj);
    lv_obj_t *caption = lv_label_create(obj);
    lv_label_set_text(caption, text);
    lv_obj_center(caption);
    return obj;
}

/* Enforce the existing 255-byte field limit, not LVGL's character limit. */
static void
limit_insert(lv_event_t *event)
{
    lv_obj_t *obj = lv_event_get_target_obj(event);
    const char *insert = lv_event_get_param(event);
    if (insert[0] != LV_KEY_DEL &&
            strlen(lv_textarea_get_text(obj)) + strlen(insert) > 255)
    {
        lv_textarea_set_insert_replace(obj, "");
    }
}

static void
clear_edits(struct xrdp_login_lvgl *ui)
{
    int i;
    for (i = 0; i < ui->edit_count; ++i)
    {
        if (ui->edits[i] != NULL)
        {
            /* The text belongs to LVGL, but is mutable storage until replaced. */
            char *text = (char *)lv_textarea_get_text(ui->edits[i]);
            OPENSSL_cleanse(text, strlen(text));
            lv_textarea_set_text(ui->edits[i], "");
        }
    }
    free(ui->edits);
    ui->edits = NULL;
    ui->edit_count = 0;
}

static int
build_fields(struct xrdp_wm *wm)
{
    struct xrdp_login_lvgl *ui = wm->login_ui;
    struct xrdp_mod_data *mod = (struct xrdp_mod_data *)list_get_item(ui->modules, ui->selected);
    char value[256];
    int i;
    lv_obj_t *focus = NULL;
    int username_set = 0;
    clear_edits(ui);
    lv_obj_clean(ui->fields);
    ui->edit_count = mod->names->count;
    ui->edits = calloc(ui->edit_count, sizeof(*ui->edits));
    if (ui->edit_count != 0 && ui->edits == NULL)
    {
        ui->edit_count = 0;
        return 1;
    }
    for (i = 0; i < ui->edit_count; ++i)
    {
        const char *name = (const char *)list_get_item(mod->names, i);
        if (xrdp_login_get_field(wm, mod, i, ui->modules->count, value))
        {
            lv_obj_t *edit;
            lv_obj_t *caption = label(ui->fields,
                                      g_strcasecmp(name, "username") == 0 ? "Username" :
                                      g_strcasecmp(name, "password") == 0 ? "Password" : name);
            lv_obj_set_style_text_font(caption, ui->log_font, 0);
            lv_obj_set_style_text_color(caption, lv_color_hex(0x596579), 0);
            lv_obj_set_style_pad_left(caption, px(ui, 6), 0);
            edit = lv_textarea_create(ui->fields);
            ui->edits[i] = edit;
            lv_obj_set_width(edit, LV_PCT(100));
            style_control(ui, edit);
            lv_textarea_set_one_line(edit, true);
            lv_obj_set_scrollbar_mode(edit, LV_SCROLLBAR_MODE_OFF);
            lv_textarea_set_password_mode(edit, xrdp_login_is_secret(name));
            lv_textarea_set_password_show_time(edit, 0);
            lv_textarea_set_text(edit, value);
            lv_obj_add_event_cb(edit, limit_insert, LV_EVENT_INSERT, NULL);
            lv_group_add_obj(ui->group, edit);
            if (focus == NULL || (username_set && xrdp_login_is_secret(name)))
            {
                focus = edit;
            }
            if (g_strcasecmp(name, "username") == 0 && value[0] != '\0')
            {
                username_set = 1;
            }
        }
        OPENSSL_cleanse(value, sizeof(value));
    }
    /* Rebuild visual focus order after switching the module. */
    if (ui->submit != NULL)
    {
        lv_obj_t *row = lv_obj_get_parent(ui->submit);
        unsigned int child;
        for (child = 0; child < lv_obj_get_child_count(row); ++child)
        {
            lv_obj_t *obj = lv_obj_get_child(row, child);
            lv_group_remove_obj(obj);
            lv_group_add_obj(ui->group, obj);
        }
    }
    if (focus != NULL)
    {
        lv_group_focus_obj(focus);
    }
    return 0;
}

static void
primary_bounds(struct xrdp_wm *wm, int *x, int *y, int *w, int *h)
{
    unsigned int i;
    *x = *y = 0;
    *w = wm->screen->width;
    *h = wm->screen->height;
    for (i = 0; i < wm->client_info->display_sizes.monitorCount; ++i)
    {
        if (wm->client_info->display_sizes.minfo_wm[i].is_primary)
        {
            *x = wm->client_info->display_sizes.minfo_wm[i].left;
            *y = wm->client_info->display_sizes.minfo_wm[i].top;
            *w = wm->client_info->display_sizes.minfo_wm[i].right - *x + 1;
            *h = wm->client_info->display_sizes.minfo_wm[i].bottom - *y + 1;
            break;
        }
    }
}

static void
place_card(struct xrdp_wm *wm)
{
    struct xrdp_login_lvgl *ui = wm->login_ui;
    int x, y, w, h;
    primary_bounds(wm, &x, &y, &w, &h);
    ui->primary_x = x;
    ui->primary_y = y;
    ui->primary_width = w;
    ui->primary_height = h;
    lv_obj_set_width(ui->card, MAX(64, MIN(px(ui, ui->log_rows ? 560 : 448), w - 24)));
    lv_obj_set_style_max_height(ui->card, MAX(64, h - 24), 0);
    lv_obj_update_layout(ui->card);
    lv_obj_set_pos(ui->card, x + (w - lv_obj_get_width(ui->card)) / 2,
                   y + MAX(0, (h - lv_obj_get_height(ui->card)) / 2));
    lv_obj_update_layout(ui->card);
}

/* Existing bitmap loading supplies BMP support without any LVGL file driver. */
static void
load_image(struct xrdp_wm *wm, const char *path, struct xrdp_bitmap **bitmap,
           lv_image_dsc_t *image, lv_obj_t *parent, enum xrdp_bitmap_load_transform transform,
           int width, int height, int transparent)
{
    lv_obj_t *obj;
    if (path[0] == '\0')
    {
        return;
    }
    *bitmap = xrdp_bitmap_create(4, 4, transparent ? 32 : 24, WND_TYPE_BITMAP, wm);
    if (xrdp_bitmap_load(*bitmap, path, wm->palette,
                         transparent ? XRDP_BITMAP_BACKGROUND_TRANSPARENT : 0xffffff,
                         transform, width, height) != 0)
    {
        xrdp_bitmap_delete(*bitmap);
        *bitmap = NULL;
        return;
    }
    image->header.magic = LV_IMAGE_HEADER_MAGIC;
    image->header.cf = transparent ? LV_COLOR_FORMAT_ARGB8888 : LV_COLOR_FORMAT_XRGB8888;
    image->header.w = (*bitmap)->width;
    image->header.h = (*bitmap)->height;
    image->header.stride = (*bitmap)->line_size;
    image->data_size = (*bitmap)->line_size * (*bitmap)->height;
    image->data = (const uint8_t *)(*bitmap)->data;
    obj = lv_image_create(parent);
    lv_image_set_src(obj, image);
    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, 0);
}

/* The rounded text face need not contain a dropdown arrow glyph. */
static void
draw_dropdown_chevron(lv_event_t *event)
{
    struct xrdp_login_lvgl *ui = lv_event_get_user_data(event);
    lv_obj_t *obj = lv_event_get_target_obj(event);
    lv_area_t area;
    lv_draw_line_dsc_t line;
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_obj_get_coords(obj, &area);
    lv_draw_line_dsc_init(&line);
    line.color = lv_color_hex(0x64748b);
    line.width = px(ui, 2);
    line.round_start = line.round_end = 1;
    line.p1.x = area.x2 - px(ui, 24);
    line.p1.y = (area.y1 + area.y2) / 2 - px(ui, 2);
    line.p2.x = area.x2 - px(ui, 19);
    line.p2.y = line.p1.y + px(ui, 5);
    lv_draw_line(layer, &line);
    line.p1 = line.p2;
    line.p2.x += px(ui, 5);
    line.p2.y -= px(ui, 5);
    lv_draw_line(layer, &line);
}

static int
create_view(struct xrdp_wm *wm, int prompt)
{
    struct xrdp_login_lvgl *ui = wm->login_ui;
    const struct xrdp_cfg_globals *cfg = &wm->xrdp_config->cfg_globals;
    lv_obj_t *screen = lv_display_get_screen_active(ui->display);
    lv_obj_t *row;
    char host[256];
    char path[512];
    int i;
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xe5eaff), 0);
    lv_obj_set_style_bg_grad_color(screen, lv_color_hex(0xf8e5ee), 0);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);
    /* Static, soft colour fields keep the software-rendered backdrop cheap. */
    for (i = 0; i < 3; ++i)
    {
        const unsigned int colors[] = {0xc3d5ff, 0xd6c9f4, 0xfbded5};
        lv_obj_t *orb = lv_obj_create(screen);
        lv_obj_remove_style_all(orb);
        lv_obj_remove_flag(orb, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(orb, LV_PCT(70), LV_PCT(85));
        lv_obj_set_style_radius(orb, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(orb, lv_color_hex(colors[i]), 0);
        lv_obj_set_style_bg_opa(orb, LV_OPA_50, 0);
        lv_obj_set_style_shadow_color(orb, lv_color_hex(colors[i]), 0);
        lv_obj_set_style_shadow_width(orb, px(ui, 80), 0);
        lv_obj_set_style_shadow_opa(orb, LV_OPA_30, 0);
        lv_obj_align(orb, i == 0 ? LV_ALIGN_TOP_LEFT :
                     i == 1 ? LV_ALIGN_RIGHT_MID : LV_ALIGN_BOTTOM_LEFT,
                     px(ui, i == 1 ? 160 : -140), px(ui, i == 0 ? -240 : 200));
    }
    lv_obj_set_style_text_font(screen, ui->font, 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(0x1c2434), 0);
    load_image(wm, cfg->ls_background_image, &ui->background, &ui->background_image,
               screen, cfg->ls_background_transform, wm->screen->width, wm->screen->height, 0);
    ui->card = lv_obj_create(screen);
    lv_obj_set_height(ui->card, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(ui->card, LV_FLEX_FLOW_COLUMN);
    style_sheet(ui, ui->card);
    lv_obj_set_flex_align(ui->card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    if (cfg->ls_logo_filename[0] != '\0')
    {
        g_strncpy(path, cfg->ls_logo_filename, sizeof(path) - 1);
    }
    else
    {
#ifdef USE_IMLIB2
        g_snprintf(path, sizeof(path), "%s/xrdp_logo.png", XRDP_SHARE_PATH);
#else
        g_snprintf(path, sizeof(path), "%s/xrdp_logo.bmp", XRDP_SHARE_PATH);
#endif
    }
    load_image(wm, path, &ui->logo, &ui->logo_image, ui->card, XBLT_SCALE,
               px(ui, 88), px(ui, 34), 1);
#ifndef USE_IMLIB2
    /* The bundled BMP has a uniform matte. Custom BMPs remain opaque. */
    if (cfg->ls_logo_filename[0] == '\0' && ui->logo != NULL)
    {
        uint32_t *pixels = (uint32_t *)ui->logo->data;
        uint32_t matte = pixels[0];
        for (i = 0; i < ui->logo->width * ui->logo->height; ++i)
        {
            if (pixels[i] == matte)
            {
                pixels[i] = 0;
            }
        }
    }
#endif
    heading(ui, ui->card, cfg->ls_title[0] ? cfg->ls_title : "Hello again.");
    ui->status = label(ui->card, prompt ? "Your workspace, wherever you are." : "Connecting…");
    lv_obj_set_style_text_color(ui->status, lv_color_hex(0x596579), 0);
    lv_obj_set_style_text_align(ui->status, LV_TEXT_ALIGN_CENTER, 0);
    if (!prompt)
    {
        ui->mode = 1;
        place_card(wm);
        return 0;
    }
    if (xrdp_login_load_modules(wm, ui->names, ui->modules) || ui->modules->count == 0)
    {
        return 1;
    }

    ui->sessions = lv_dropdown_create(ui->card);
    style_control(ui, ui->sessions);
    lv_dropdown_set_symbol(ui->sessions, NULL);
    style_control(ui, lv_dropdown_get_list(ui->sessions));
    lv_obj_set_style_pad_right(ui->sessions, px(ui, 36), 0);
    lv_obj_add_event_cb(ui->sessions, draw_dropdown_chevron, LV_EVENT_DRAW_MAIN, ui);
    lv_dropdown_clear_options(ui->sessions);
    for (i = 0; i < ui->names->count; ++i)
    {
        lv_dropdown_add_option(ui->sessions, (const char *)list_get_item(ui->names, i), LV_DROPDOWN_POS_LAST);
    }
    lv_obj_set_width(ui->sessions, LV_PCT(100));
    ui->selected = xrdp_login_parse_domain(wm->client_info->domain, ui->modules->count, 1, host, sizeof(host));
    lv_dropdown_set_selected(ui->sessions, ui->selected);
    lv_obj_set_user_data(ui->sessions, (void *)(intptr_t)UI_SELECT);
    lv_obj_add_event_cb(ui->sessions, request_action, LV_EVENT_VALUE_CHANGED, ui);
    lv_group_add_obj(ui->group, ui->sessions);
    ui->fields = lv_obj_create(ui->card);
    lv_obj_remove_style_all(ui->fields);
    lv_obj_set_size(ui->fields, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(ui->fields, LV_FLEX_FLOW_COLUMN);
    /* Keep focus outlines inside the transparent containers' clipping bounds. */
    lv_obj_set_style_pad_all(ui->fields, px(ui, 6), 0);
    lv_obj_set_style_pad_row(ui->fields, px(ui, 10), 0);
    /* Resolve field widths before prefilling text and positioning cursors. */
    place_card(wm);
    if (build_fields(wm))
    {
        return 1;
    }
    row = lv_obj_create(ui->card);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_all(row, px(ui, 6), 0);
    lv_obj_set_style_pad_gap(row, px(ui, 8), 0);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    ui->submit = button(ui, row, "Continue", UI_SUBMIT);
    button(ui, row, "Cancel", UI_CANCEL);
    button(ui, row, "Need help?", UI_HELP);
    place_card(wm);
    return 0;
}

int
xrdp_login_lvgl_create(struct xrdp_wm *wm, int prompt)
{
    struct xrdp_login_lvgl *ui;
    FcPattern *pattern;
    FcPattern *match;
    FcResult result;
    FcChar8 *font_path;
    char path[1024];
    int width = wm->screen->width;
    int height = wm->screen->height;
    int failed;
    const char *failure = "display or input allocation";
    unsigned int dpi;
    if (wm->login_ui != NULL)
    {
        return 0;
    }
    if (wm->login_ui_failed)
    {
        return 1;
    }
    if (width <= 0 || height <= 0 ||
            (size_t)width * height > MAX_UI_PIXELS ||
            (wm->screen->bpp != 15 && wm->screen->bpp != 16 &&
             wm->screen->bpp != 24 && wm->screen->bpp != 32))
    {
        LOG(LOG_LEVEL_WARNING, "LVGL does not support login geometry %dx%d at %d bpp; using legacy",
            width, height, wm->screen->bpp);
        wm->login_ui_failed = 1;
        return 1;
    }
    ui = calloc(1, sizeof(*ui));
    if (ui == NULL)
    {
        LOG(LOG_LEVEL_WARNING, "Unable to allocate LVGL login state; using legacy");
        wm->login_ui_failed = 1;
        return 1;
    }
    ui->event = g_create_wait_obj(NULL);
    ui->names = list_create();
    ui->names->auto_free = 1;
    ui->modules = list_create();
    ui->frame = xrdp_bitmap_create(width, height, wm->screen->bpp, WND_TYPE_BITMAP, wm);
    ui->draw_buffer = malloc((size_t)(width * 4 + 64) * 32);
    wm->login_ui = ui;
    pthread_mutex_lock(&ui_mutex);
    if (ui_users++ == 0)
    {
        lv_init();
        font_config = FcInitLoadConfigAndFonts();
        lv_tick_set_cb(g_get_elapsed_ms);
        timer_due = g_get_elapsed_ms();
    }
    g_strncpy(path, wm->xrdp_config->cfg_globals.ls_font_file, sizeof(path) - 1);
    if (path[0] == '\0' && font_config != NULL)
    {
        pattern = FcNameParse((const FcChar8 *)"Quicksand,Nunito Sans,sans-serif");
        if (pattern != NULL)
        {
            FcConfigSubstitute(font_config, pattern, FcMatchPattern);
            FcDefaultSubstitute(pattern);
            match = FcFontMatch(font_config, pattern, &result);
            if (match != NULL)
            {
                if (FcPatternGetString(match, FC_FILE, 0, &font_path) == FcResultMatch)
                {
                    g_strncpy(path, (const char *)font_path, sizeof(path) - 1);
                }
                FcPatternDestroy(match);
            }
            FcPatternDestroy(pattern);
        }
    }
    dpi = xrdp_login_wnd_get_monitor_dpi(wm);
    if (dpi == 0)
    {
        dpi = wm->xrdp_config->cfg_globals.default_dpi;
    }
    dpi = MAX(96, MIN(384, dpi));
    ui->dpi = dpi;
    ui->font = lv_freetype_font_create(path, LV_FREETYPE_FONT_RENDER_MODE_BITMAP,
                                       17 * dpi / 96, LV_FREETYPE_FONT_STYLE_NORMAL);
    ui->heading_font = lv_freetype_font_create(path, LV_FREETYPE_FONT_RENDER_MODE_BITMAP,
                       36 * dpi / 96, LV_FREETYPE_FONT_STYLE_BOLD);
    ui->log_font = lv_freetype_font_create(path, LV_FREETYPE_FONT_RENDER_MODE_BITMAP,
                                           16 * dpi / 96, LV_FREETYPE_FONT_STYLE_NORMAL);
    failed = ui->event == 0 || ui->frame == NULL || ui->draw_buffer == NULL ||
             ui->font == NULL || ui->heading_font == NULL || ui->log_font == NULL;
    if (failed)
    {
        failure = ui->font == NULL || ui->heading_font == NULL || ui->log_font == NULL ?
                  "FreeType font initialization" : "framebuffer or wake event allocation";
    }
    if (!failed)
    {
        ui->display = lv_display_create(width, height);
        ui->group = lv_group_create();
        ui->pointer = lv_indev_create();
        failed = ui->display == NULL || ui->group == NULL || ui->pointer == NULL;
    }
    if (!failed)
    {
        lv_display_set_user_data(ui->display, ui);
        lv_display_set_color_format(ui->display, LV_COLOR_FORMAT_XRGB8888);
        lv_display_set_buffers(ui->display, ui->draw_buffer, NULL,
                               (width * 4 + 64) * 32, LV_DISPLAY_RENDER_MODE_PARTIAL);
        lv_display_set_flush_cb(ui->display, flush_pixels);
        lv_display_set_dpi(ui->display, dpi);
        lv_indev_set_type(ui->pointer, LV_INDEV_TYPE_POINTER);
        lv_indev_set_display(ui->pointer, ui->display);
        lv_indev_set_group(ui->pointer, ui->group);
        lv_indev_set_user_data(ui->pointer, ui);
        lv_indev_set_read_cb(ui->pointer, read_pointer);
        lv_indev_set_mode(ui->pointer, LV_INDEV_MODE_EVENT);
        /* Always create objects under this connection's explicit screen. */
        failure = "session configuration or form allocation";
        failed = create_view(wm, prompt);
        refresh_soon(ui);
    }
    pthread_mutex_unlock(&ui_mutex);
    if (failed)
    {
        xrdp_login_lvgl_delete(wm);
        wm->login_ui_failed = 1;
        LOG(LOG_LEVEL_WARNING, "Unable to initialize LVGL login: %s; using legacy", failure);
        return 1;
    }
    return 0;
}

void
xrdp_login_lvgl_delete(struct xrdp_wm *wm)
{
    struct xrdp_login_lvgl *ui = wm->login_ui;
    if (ui == NULL)
    {
        return;
    }
    pthread_mutex_lock(&ui_mutex);
    clear_edits(ui);
    if (ui->pointer != NULL)
    {
        lv_indev_delete(ui->pointer);
    }
    if (ui->display != NULL)
    {
        lv_display_delete(ui->display);
    }
    lv_image_cache_drop(&ui->logo_image);
    lv_image_cache_drop(&ui->background_image);
    if (ui->group != NULL)
    {
        lv_group_delete(ui->group);
    }
    if (ui->font != NULL)
    {
        lv_freetype_font_delete(ui->font);
    }
    if (ui->heading_font != NULL)
    {
        lv_freetype_font_delete(ui->heading_font);
    }
    if (ui->log_font != NULL)
    {
        lv_freetype_font_delete(ui->log_font);
    }
    if (--ui_users == 0)
    {
        lv_deinit();
        FcConfigDestroy(font_config);
        font_config = NULL;
    }
    pthread_mutex_unlock(&ui_mutex);
    xrdp_login_free_modules(ui->modules);
    list_delete(ui->names);
    xrdp_bitmap_delete(ui->frame);
    xrdp_bitmap_delete(ui->logo);
    xrdp_bitmap_delete(ui->background);
    free(ui->draw_buffer);
    g_delete_wait_obj(ui->event);
    free(ui);
    wm->login_ui = NULL;
}

/* Called with the toolkit mutex held. Rows survive the progress/error switch. */
static void
create_log_view(struct xrdp_login_lvgl *ui)
{
    clear_edits(ui);
    if (ui->help != NULL)
    {
        lv_obj_delete(ui->help);
    }
    lv_obj_remove_flag(ui->card, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_state(ui->card, LV_STATE_DISABLED);
    lv_obj_clean(ui->card);
    ui->help = ui->fields = ui->sessions = ui->submit = NULL;
    ui->log_title = heading(ui, ui->card, "Almost there.");
    ui->status = label(ui->card, "Getting your workspace ready.");
    lv_obj_set_style_text_color(ui->status, lv_color_hex(0x64748b), 0);
    lv_obj_set_style_text_font(ui->status, ui->log_font, 0);
    lv_obj_set_style_text_align(ui->status, LV_TEXT_ALIGN_CENTER, 0);
    ui->log_rows = lv_obj_create(ui->card);
    lv_obj_remove_style_all(ui->log_rows);
    lv_obj_set_size(ui->log_rows, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(ui->log_rows, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(ui->log_rows, px(ui, 8), 0);
    ui->mode = 1;
    ui->action = UI_NONE;
}

enum log_mark { LOG_MARK_PROGRESS, LOG_MARK_SUCCESS, LOG_MARK_ERROR, LOG_MARK_WARNING };

static enum log_mark
log_mark_for(int level, const char *message)
{
    if (level == LOG_LEVEL_ERROR)
    {
        return LOG_MARK_ERROR;
    }
    if (level == LOG_LEVEL_WARNING)
    {
        return LOG_MARK_WARNING;
    }
    /* Only known completed steps get a check, not every INFO message. */
    if (level == LOG_LEVEL_INFO &&
            (!strcmp(message, "sesman connect ok") ||
             !strcmp(message, "access control check was successful") ||
             !strcmp(message, "login was successful - creating session") ||
             !strcmp(message, "Got connection details for session") ||
             !strncmp(message, "session is available on display ", 32)))
    {
        return LOG_MARK_SUCCESS;
    }
    return LOG_MARK_PROGRESS;
}

/* Draw the markers so system fonts need no check/cross/icon glyph coverage. */
static void
draw_log_mark(lv_event_t *event)
{
    enum log_mark mark = (intptr_t)lv_event_get_user_data(event);
    lv_obj_t *obj = lv_event_get_target_obj(event);
    lv_area_t area;
    lv_draw_line_dsc_t line;
    lv_layer_t *layer = lv_event_get_layer(event);
    int unit = lv_obj_get_width(obj);
    lv_obj_get_coords(obj, &area);
    lv_draw_line_dsc_init(&line);
    line.color = lv_obj_get_style_text_color(obj, 0);
    line.width = MAX(2, unit / 10);
    line.round_start = line.round_end = 1;
    line.p1.x = area.x1 + unit * 3 / 10;
    line.p1.y = area.y1 + unit * (mark == LOG_MARK_SUCCESS ? 5 : 3) / 10;
    line.p2.x = area.x1 + unit * (mark == LOG_MARK_SUCCESS ? 4 : 7) / 10;
    line.p2.y = area.y1 + unit * 7 / 10;
    if (mark == LOG_MARK_PROGRESS || mark == LOG_MARK_WARNING)
    {
        line.p1.x = line.p2.x = area.x1 + unit / 2;
        line.p1.y = area.y1 + unit * (mark == LOG_MARK_WARNING ? 3 : 5) / 10;
        line.p2.y = area.y1 + unit / 2;
        line.width = MAX(2, unit / (mark == LOG_MARK_PROGRESS ? 5 : 10));
        if (mark == LOG_MARK_PROGRESS)
        {
            line.p2.x++; /* A zero-length line is skipped by the renderer. */
        }
    }
    lv_draw_line(layer, &line);
    if (mark != LOG_MARK_PROGRESS)
    {
        line.p1 = line.p2;
        line.p2.x = area.x1 + unit * 7 / 10;
        line.p2.y = area.y1 + unit * 3 / 10;
        if (mark == LOG_MARK_ERROR)
        {
            line.p1.x = area.x1 + unit * 3 / 10;
        }
        else if (mark == LOG_MARK_WARNING)
        {
            line.p1.y = line.p2.y = area.y1 + unit * 7 / 10;
            line.p2.x = line.p1.x + 1;
        }
        lv_draw_line(layer, &line);
    }
}

void
xrdp_login_lvgl_progress(struct xrdp_wm *wm)
{
    struct xrdp_login_lvgl *ui = wm->login_ui;
    if (ui == NULL)
    {
        return;
    }
    pthread_mutex_lock(&ui_mutex);
    create_log_view(ui);
    place_card(wm);
    refresh_soon(ui);
    pthread_mutex_unlock(&ui_mutex);
}

void
xrdp_login_lvgl_log_message(struct xrdp_wm *wm, int level, const char *message)
{
    struct xrdp_login_lvgl *ui = wm->login_ui;
    enum log_mark mark = log_mark_for(level, message);
    const unsigned int colors[] = {0x64748b, 0x15803d, 0xb91c1c, 0xa16207};
    const unsigned int backgrounds[] = {0xf1f5f9, 0xf0fdf4, 0xfef2f2, 0xfffbeb};
    lv_obj_t *row;
    lv_obj_t *icon;
    lv_obj_t *text;
    if (ui == NULL || wm->hide_log_window)
    {
        return;
    }
    pthread_mutex_lock(&ui_mutex);
    if (ui->log_rows == NULL)
    {
        create_log_view(ui);
    }
    row = lv_obj_create(ui->log_rows);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(row, px(ui, 12), 0);
    lv_obj_set_style_pad_column(row, px(ui, 10), 0);
    lv_obj_set_style_bg_color(row, lv_color_hex(backgrounds[mark]), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(row, px(ui, 18), 0);
    icon = lv_obj_create(row);
    lv_obj_remove_style_all(icon);
    lv_obj_set_size(icon, px(ui, 22), px(ui, 22));
    lv_obj_set_style_text_color(icon, lv_color_hex(colors[mark]), 0);
    lv_obj_add_event_cb(icon, draw_log_mark, LV_EVENT_DRAW_MAIN, (void *)(intptr_t)mark);
    text = label(row, message);
    lv_obj_set_width(text, 0);
    lv_obj_set_flex_grow(text, 1);
    lv_obj_set_style_text_font(text, ui->log_font, 0);
    lv_obj_set_style_text_color(text, lv_color_hex(0x334155), 0);
    lv_obj_set_style_text_line_space(text, px(ui, 5), 0);
    place_card(wm);
    lv_obj_scroll_to_view(row, LV_ANIM_OFF);
    refresh_soon(ui);
    pthread_mutex_unlock(&ui_mutex);
}

void
xrdp_login_lvgl_log(struct xrdp_wm *wm, int error)
{
    struct xrdp_login_lvgl *ui = wm->login_ui;
    if (ui == NULL || !error)
    {
        return;
    }
    pthread_mutex_lock(&ui_mutex);
    if (ui->log_rows == NULL)
    {
        create_log_view(ui);
    }
    lv_label_set_text(ui->log_title, wm->fatal_error_in_log_window ? "Connection ended" : "Unable to connect");
    lv_label_set_text(ui->status, wm->fatal_error_in_log_window ?
                      "Review the details below before closing." : "Review the details below, then try again.");
    if (ui->mode != 2)
    {
        lv_group_focus_obj(button(ui, ui->card, wm->fatal_error_in_log_window ? "Close" : "Try again", UI_ACK));
        ui->mode = 2;
    }
    place_card(wm);
    refresh_soon(ui);
    pthread_mutex_unlock(&ui_mutex);
}

void
xrdp_login_lvgl_invalidate(struct xrdp_wm *wm, const struct xrdp_rect *rect)
{
    struct xrdp_login_lvgl *ui = wm->login_ui;
    if (ui == NULL)
    {
        return;
    }
    pthread_mutex_lock(&ui_mutex);
    if (rect != NULL)
    {
        mark_dirty(ui, rect->left, rect->top, rect->right, rect->bottom);
    }
    else
    {
        mark_dirty(ui, 0, 0, ui->frame->width, ui->frame->height);
    }
    pthread_mutex_unlock(&ui_mutex);
}

void
xrdp_login_lvgl_mouse(struct xrdp_wm *wm, int x, int y, int but, int down)
{
    struct xrdp_login_lvgl *ui = wm->login_ui;
    pthread_mutex_lock(&ui_mutex);
    /* Wheel events arrive as button 4..7 with down=0 and no coordinates. */
    if (but >= 4 && but <= 7)
    {
        lv_obj_t *obj = ui->help != NULL ? ui->help : ui->card;
        if (ui->sessions != NULL && lv_dropdown_is_open(ui->sessions))
        {
            obj = lv_dropdown_get_list(ui->sessions);
        }
        lv_obj_scroll_by(obj,
                         but == 6 ? px(ui, 48) : (but == 7 ? -px(ui, 48) : 0),
                         but == 4 ? px(ui, 48) : (but == 5 ? -px(ui, 48) : 0),
                         LV_ANIM_OFF);
    }
    else
    {
        ui->mouse.x = x;
        ui->mouse.y = y;
        if (but == 1)
        {
            ui->pressed = down;
        }
        lv_indev_read(ui->pointer);
    }
    timer_due = g_get_elapsed_ms();
    pthread_mutex_unlock(&ui_mutex);
}

void
xrdp_login_lvgl_key(struct xrdp_wm *wm, int sym, unsigned int chr, int down, int shift)
{
    struct xrdp_login_lvgl *ui = wm->login_ui;
    lv_obj_t *focus;
    uint32_t key = 0;
    char text[8] = {0};
    if (!down)
    {
        return;
    }
    if (sym == 0)
    {
        switch (chr)
        {
            case '\b':
                sym = 0xff08;
                break;
            case '\t':
                sym = 0xff09;
                break;
            case '\r':
                sym = 0xff0d;
                break;
            case 0x1b:
                sym = 0xff1b;
                break;
            case 0x7f:
                sym = 0xffff;
                break;
        }
    }
    pthread_mutex_lock(&ui_mutex);
    focus = lv_group_get_focused(ui->group);
    switch (sym)
    {
        case 0xff09: /* Tab */
        case 0xfe20: /* ISO_Left_Tab */
            if (shift || sym == 0xfe20)
            {
                lv_group_focus_prev(ui->group);
            }
            else
            {
                lv_group_focus_next(ui->group);
            }
            break;
        case 0xff1b: /* Escape */
            if (ui->sessions && lv_dropdown_is_open(ui->sessions))
            {
                lv_dropdown_close(ui->sessions);
            }
            else
            {
                ui->action = ui->help ? UI_CLOSE_HELP : (ui->mode == 2 ? UI_ACK : UI_CANCEL);
            }
            break;
        case 0xff0d: /* Return */
        case 0xff8d: /* KP_Enter */
            if (focus && lv_obj_check_type(focus, &lv_button_class))
            {
                lv_obj_send_event(focus, LV_EVENT_CLICKED, NULL);
            }
            else if (focus == ui->sessions && focus != NULL)
            {
                lv_group_send_data(ui->group, LV_KEY_ENTER);
            }
            else if (ui->mode == 0 && ui->help == NULL)
            {
                ui->action = UI_SUBMIT;
            }
            break;
        case 0xff08:
            key = LV_KEY_BACKSPACE;
            break;
        case 0xffff:
            key = LV_KEY_DEL;
            break;
        case 0xff51:
            key = LV_KEY_LEFT;
            break;
        case 0xff53:
            key = LV_KEY_RIGHT;
            break;
        case 0xff52:
            key = LV_KEY_UP;
            break;
        case 0xff54:
            key = LV_KEY_DOWN;
            break;
        case 0xff50:
            key = LV_KEY_HOME;
            break;
        case 0xff57:
            key = LV_KEY_END;
            break;
        default:
            if (focus && lv_obj_check_type(focus, &lv_textarea_class) && chr >= 32 && chr != 127)
            {
                utf8_add_char_at(text, sizeof(text), chr, 0);
                lv_textarea_add_text(focus, text);
                OPENSSL_cleanse(text, sizeof(text));
            }
            else if (focus && chr == ' ' && lv_obj_check_type(focus, &lv_button_class))
            {
                lv_obj_send_event(focus, LV_EVENT_CLICKED, NULL);
            }
            break;
    }
    if (key)
    {
        lv_group_send_data(ui->group, key);
    }
    if (ui->action)
    {
        g_set_wait_obj(ui->event);
    }
    timer_due = g_get_elapsed_ms();
    pthread_mutex_unlock(&ui_mutex);
}

void
xrdp_login_lvgl_wait(struct xrdp_wm *wm, tbus *objs, int *count, int *timeout)
{
    int remaining;
    if (wm->login_ui == NULL)
    {
        return;
    }
    objs[(*count)++] = wm->login_ui->event;
    pthread_mutex_lock(&ui_mutex);
    remaining = MAX(0, (int)(timer_due - g_get_elapsed_ms()));
    if (*timeout < 0 || *timeout > remaining)
    {
        *timeout = remaining;
    }
    pthread_mutex_unlock(&ui_mutex);
}

static int
handle_action(struct xrdp_wm *wm, int action)
{
    struct xrdp_login_lvgl *ui = wm->login_ui;
    struct xrdp_mod_data *mod;
    int i;
    int rv = 0;
    if (action == UI_CANCEL)
    {
        wm->pro_layer->errinfo = ERRINFO_SERVER_INSUFFICIENT_PRIVILEGES;
        g_set_wait_obj(wm->pro_layer->self_term_event);
        return 0;
    }
    if (action == UI_ACK)
    {
        if (wm->fatal_error_in_log_window || wm->mm->mod_handle == 0)
        {
            wm->client_info->rdp_autologin = 0;
            xrdp_wm_set_login_state(wm, WMLS_RESET);
        }
        else
        {
            xrdp_login_lvgl_delete(wm);
            xrdp_bitmap_invalidate(wm->screen, NULL);
        }
        return 0;
    }
    pthread_mutex_lock(&ui_mutex);
    if (action == UI_SUBMIT && ui->mode == 0 && wm->login_state == WMLS_USER_PROMPT)
    {
        mod = (struct xrdp_mod_data *)list_get_item(ui->modules, ui->selected);
        for (i = 0; i < ui->edit_count; ++i)
        {
            if (ui->edits[i] != NULL)
            {
                xrdp_login_set_value(mod, (const char *)list_get_item(mod->names, i),
                                     lv_textarea_get_text(ui->edits[i]));
            }
        }
        /* Copy and signal from the owner, never from an LVGL callback. */
        xrdp_login_submit(wm, mod);
        clear_edits(ui);
        lv_obj_add_state(ui->card, LV_STATE_DISABLED);
        ui->mode = 1;
    }
    else if (action == UI_SELECT && ui->mode == 0)
    {
        ui->selected = lv_dropdown_get_selected(ui->sessions);
        rv = build_fields(wm);
        place_card(wm);
    }
    else if (action == UI_HELP && ui->help == NULL)
    {
        ui->help = lv_obj_create(lv_display_get_screen_active(ui->display));
        lv_obj_set_size(ui->help, lv_obj_get_width(ui->card), LV_SIZE_CONTENT);
        lv_obj_set_style_max_height(ui->help, ui->frame->height - 16, 0);
        lv_obj_set_flex_flow(ui->help, LV_FLEX_FLOW_COLUMN);
        style_sheet(ui, ui->help);
        heading(ui, ui->help, "A little help.");
        label(ui->help, "Choose a session and enter your credentials. Usernames and passwords are case sensitive. Contact your system administrator if you cannot sign in.");
        lv_group_focus_obj(button(ui, ui->help, "Back", UI_CLOSE_HELP));
        lv_obj_add_flag(ui->card, LV_OBJ_FLAG_HIDDEN);
        lv_obj_center(ui->help);
    }
    else if (action == UI_CLOSE_HELP && ui->help != NULL)
    {
        lv_obj_delete(ui->help);
        ui->help = NULL;
        lv_obj_remove_flag(ui->card, LV_OBJ_FLAG_HIDDEN);
        lv_group_focus_obj(ui->sessions);
    }
    refresh_soon(ui);
    pthread_mutex_unlock(&ui_mutex);
    return rv;
}

int
xrdp_login_lvgl_check(struct xrdp_wm *wm)
{
    struct xrdp_login_lvgl *ui = wm->login_ui;
    struct xrdp_bitmap *snapshot = NULL;
    struct xrdp_rect rect;
    unsigned int now;
    int action;
    int rv = 0;
    if (ui == NULL)
    {
        return 0;
    }
    if (ui->frame->width != wm->screen->width || ui->frame->height != wm->screen->height ||
            ui->frame->bpp != wm->screen->bpp)
    {
        int width = wm->screen->width;
        int height = wm->screen->height;
        struct xrdp_bitmap *frame;
        void *buffer;
        if (width <= 0 || height <= 0 || (size_t)width * height > MAX_UI_PIXELS ||
                (wm->screen->bpp != 15 && wm->screen->bpp != 16 &&
                 wm->screen->bpp != 24 && wm->screen->bpp != 32))
        {
            return 1;
        }
        buffer = malloc((size_t)(width * 4 + 64) * 32);
        if (buffer == NULL)
        {
            return 1;
        }
        frame = xrdp_bitmap_create(width, height, wm->screen->bpp, WND_TYPE_BITMAP, wm);
        if (frame == NULL)
        {
            free(buffer);
            return 1;
        }
        pthread_mutex_lock(&ui_mutex);
        lv_display_set_buffers(ui->display, buffer, NULL,
                               (width * 4 + 64) * 32, LV_DISPLAY_RENDER_MODE_PARTIAL);
        free(ui->draw_buffer);
        ui->draw_buffer = buffer;
        xrdp_bitmap_delete(ui->frame);
        ui->frame = frame;
        ui->dirty = 0;
        lv_display_set_resolution(ui->display, width, height);
        place_card(wm);
        lv_obj_invalidate(lv_display_get_screen_active(ui->display));
        refresh_soon(ui);
        pthread_mutex_unlock(&ui_mutex);
    }
    pthread_mutex_lock(&ui_mutex);
    {
        int x, y, w, h;
        primary_bounds(wm, &x, &y, &w, &h);
        if (x != ui->primary_x || y != ui->primary_y ||
                w != ui->primary_width || h != ui->primary_height)
        {
            place_card(wm);
            refresh_soon(ui);
        }
    }
    g_reset_wait_obj(ui->event);
    now = g_get_elapsed_ms();
    if ((int)(now - timer_due) >= 0)
    {
        uint32_t delay = lv_timer_handler();
        timer_due = now + MIN(delay, INT_MAX);
    }
    action = ui->action;
    ui->action = UI_NONE;
    if (ui->dirty)
    {
        rect = ui->damage;
        snapshot = xrdp_bitmap_create(rect.right - rect.left, rect.bottom - rect.top,
                                      ui->frame->bpp, WND_TYPE_BITMAP, wm);
        if (snapshot != NULL)
        {
            xrdp_bitmap_copy_box(ui->frame, snapshot, rect.left, rect.top,
                                 snapshot->width, snapshot->height);
            ui->dirty = 0;
        }
    }
    pthread_mutex_unlock(&ui_mutex);
    if (snapshot != NULL)
    {
        xrdp_painter_begin_update(wm->painter);
        rv = xrdp_painter_copy(wm->painter, snapshot, wm->screen, rect.left, rect.top,
                               snapshot->width, snapshot->height, 0, 0);
        xrdp_painter_end_update(wm->painter);
        xrdp_bitmap_delete(snapshot);
    }
    if (rv == 0 && action != UI_NONE)
    {
        rv = handle_action(wm, action);
    }
    return rv;
}

#else /* A legacy-only build has no toolkit dependency. */
int xrdp_login_lvgl_create(struct xrdp_wm *wm, int prompt)
{
    (void)prompt;
    if (!wm->login_ui_failed)
    {
        LOG(LOG_LEVEL_WARNING, "ls_ui=lvgl requested, but xrdp was built without LVGL; using legacy");
    }
    wm->login_ui_failed = 1;
    return 1;
}
void xrdp_login_lvgl_delete(struct xrdp_wm *wm)
{
    (void)wm;
}
void xrdp_login_lvgl_progress(struct xrdp_wm *wm)
{
    (void)wm;
}
void xrdp_login_lvgl_log(struct xrdp_wm *wm, int error)
{
    (void)wm;
    (void)error;
}
void xrdp_login_lvgl_log_message(struct xrdp_wm *wm, int level, const char *message)
{
    (void)wm;
    (void)level;
    (void)message;
}
void xrdp_login_lvgl_invalidate(struct xrdp_wm *wm, const struct xrdp_rect *rect)
{
    (void)wm;
    (void)rect;
}
void xrdp_login_lvgl_mouse(struct xrdp_wm *wm, int x, int y, int b, int d)
{
    (void)wm;
    (void)x;
    (void)y;
    (void)b;
    (void)d;
}
void xrdp_login_lvgl_key(struct xrdp_wm *wm, int s, unsigned int c, int d, int shift)
{
    (void)wm;
    (void)s;
    (void)c;
    (void)d;
    (void)shift;
}
void xrdp_login_lvgl_wait(struct xrdp_wm *wm, tbus *o, int *c, int *t)
{
    (void)wm;
    (void)o;
    (void)c;
    (void)t;
}
int xrdp_login_lvgl_check(struct xrdp_wm *wm)
{
    (void)wm;
    return 0;
}
#endif
