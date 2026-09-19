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
 */

#include "test_xrdp.h"
/* Compile the adapter here instead of linking its object, so tests can check
 * its private buffers and callbacks without exporting a test API. */
#include "../../xrdp/xrdp_login_lvgl.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct login_fixture
{
    struct xrdp_wm wm;
    struct xrdp_config config;
    struct xrdp_client_info client;
    struct xrdp_session session;
    struct xrdp_mm mm;
    struct xrdp_process process;
    char ini[64];
};

static void
init_fixture(struct login_fixture *f, int bpp)
{
    int fd;
    const char *contents = "[Globals]\nls_ui=lvgl\n"
                           "[Xorg]\nname=Desktop\nusername=ask\npassword=ask\nip=ask\n"
                           "port=ask3389\nfixed={base64}aGVsbG8=\n"
                           "[VNC]\nusername=askalice\npampassword=ask{base64}c2VjcmV0\n";
    memset(f, 0, sizeof(*f));
    strcpy(f->ini, "/tmp/xrdp-login-test-XXXXXX");
    fd = mkstemp(f->ini);
    ck_assert_int_ge(fd, 0);
    ck_assert_int_eq(write(fd, contents, strlen(contents)), strlen(contents));
    close(fd);
    f->wm.session = &f->session;
    f->session.xrdp_ini = f->ini;
    f->session.client_info = &f->client;
    f->wm.client_info = &f->client;
    f->wm.xrdp_config = &f->config;
    f->wm.mm = &f->mm;
    f->wm.pro_layer = &f->process;
    f->wm.log = list_create();
    f->wm.log->auto_free = 1;
    f->mm.login_names = list_create();
    f->mm.login_names->auto_free = 1;
    f->mm.login_values = list_create();
    f->mm.login_values->auto_free = 1;
    f->wm.login_state = WMLS_USER_PROMPT;
    f->wm.login_state_event = g_create_wait_obj(NULL);
    f->process.self_term_event = g_create_wait_obj(NULL);
    f->wm.screen = xrdp_bitmap_create(800, 600, bpp, WND_TYPE_SCREEN, &f->wm);
    ck_assert_int_eq(load_xrdp_config(&f->config, f->ini, bpp), 0);
}

static void
free_fixture(struct login_fixture *f)
{
    xrdp_login_lvgl_delete(&f->wm);
    xrdp_bitmap_delete(f->wm.screen);
    list_delete(f->wm.log);
    list_delete(f->mm.login_names);
    list_delete(f->mm.login_values);
    g_delete_wait_obj(f->wm.login_state_event);
    g_delete_wait_obj(f->process.self_term_event);
    unlink(f->ini);
}

START_TEST(test_login_model)
{
    struct login_fixture f;
    struct list *names = list_create();
    struct list *modules = list_create();
    struct xrdp_mod_data *mod;
    char value[256];
    char host[256];
    char encoded[521] = "{base64}";
    int i;
    names->auto_free = 1;
    init_fixture(&f, 24);
    strcpy(f.client.username, "prefilled");
    strcpy(f.client.domain, "_gateway.example__1");
    ck_assert_int_eq(f.config.cfg_globals.ls_ui, 1);
    ck_assert_int_eq(xrdp_login_load_modules(&f.wm, names, modules), 0);
    ck_assert_int_eq(modules->count, 2);
    ck_assert_str_eq((const char *)list_get_item(names, 0), "Desktop");
    ck_assert_int_eq(xrdp_login_parse_domain(f.client.domain, 2, 1, host, sizeof(host)), 1);
    ck_assert_str_eq(host, "gateway.example");
    mod = (struct xrdp_mod_data *)list_get_item(modules, 0);
    for (i = 0; i < mod->names->count; ++i)
    {
        const char *name = (const char *)list_get_item(mod->names, i);
        int editable = xrdp_login_get_field(&f.wm, mod, i, 2, value);
        if (strcmp(name, "username") == 0)
        {
            ck_assert_str_eq(value, "prefilled");
        }
        if (strcmp(name, "ip") == 0)
        {
            ck_assert_str_eq(value, "gateway.example");
        }
        if (strcmp(name, "port") == 0)
        {
            ck_assert_str_eq(value, "3389");
        }
        if (strcmp(name, "fixed") == 0)
        {
            ck_assert_int_eq(editable, 0);
            ck_assert_str_eq(value, "hello");
            ck_assert_str_eq((const char *)list_get_item(mod->values, i), "hello");
        }
    }
    for (i = 0; i < 128; ++i)
    {
        strcat(encoded, "YWFh");
    }
    xrdp_login_set_value(mod, "fixed", encoded);
    ck_assert_int_eq(xrdp_login_get_field(&f.wm, mod, 5, 2, value), 0);
    ck_assert_int_eq(strlen((const char *)list_get_item(mod->values, 5)), 384);
    mod = (struct xrdp_mod_data *)list_get_item(modules, 1);
    ck_assert_int_eq(xrdp_login_get_field(&f.wm, mod, 1, 2, value), 1);
    ck_assert_str_eq(value, "secret");
    ck_assert(xrdp_login_is_secret("PAMPassword"));
    xrdp_login_set_value(mod, "username", "alice");
    xrdp_login_submit(&f.wm, mod);
    ck_assert_int_eq(f.wm.login_state, WMLS_START_CONNECT);
    ck_assert_str_eq((const char *)list_get_item(f.mm.login_values, 0), "alice");
    xrdp_login_set_value(mod, "username", "other");
    xrdp_login_submit(&f.wm, mod); /* Duplicate submissions are ignored. */
    ck_assert_str_eq((const char *)list_get_item(f.mm.login_values, 0), "alice");
    xrdp_login_free_modules(modules);
    list_delete(names);
    free_fixture(&f);
}
END_TEST

#ifdef XRDP_LVGL
START_TEST(test_lvgl_logo_alpha)
{
    struct login_fixture f;
    struct xrdp_login_lvgl *ui;
    uint32_t *pixels;
    int i;
    int visible = 0;
    init_fixture(&f, 32);
    ck_assert_int_eq(xrdp_login_lvgl_create(&f.wm, 1), 0);
    ui = f.wm.login_ui;
    ck_assert_ptr_nonnull(ui->logo);
    ck_assert_int_eq(ui->logo_image.header.cf, LV_COLOR_FORMAT_ARGB8888);
    pixels = (uint32_t *)ui->logo->data;
    ck_assert_uint_eq(pixels[0] >> 24, 0);
    for (i = 0; i < ui->logo->width * ui->logo->height; ++i)
    {
        visible += (pixels[i] >> 24) != 0;
    }
    ck_assert_int_gt(visible, 0);
    free_fixture(&f);

    /* The matte in a custom BMP is content, not a transparency key. */
    init_fixture(&f, 32);
    g_strncpy(f.config.cfg_globals.ls_logo_filename,
              XRDP_SHARE_PATH "/xrdp_logo.bmp", 255);
    ck_assert_int_eq(xrdp_login_lvgl_create(&f.wm, 1), 0);
    ui = f.wm.login_ui;
    ck_assert_ptr_nonnull(ui->logo);
    ck_assert_uint_eq(((uint32_t *)ui->logo->data)[0] >> 24, 255);
    free_fixture(&f);
}
END_TEST

START_TEST(test_lvgl_focus_outline)
{
    struct login_fixture f;
    struct xrdp_login_lvgl *ui;
    int i;
    init_fixture(&f, 32);
    strcpy(f.client.username, "lvgltest");
    ck_assert_int_eq(xrdp_bitmap_resize(f.wm.screen, 1280, 1000), 0);
    ck_assert_int_eq(xrdp_login_lvgl_create(&f.wm, 1), 0);
    ui = f.wm.login_ui;
    lv_obj_t *controls[] = {ui->edits[1], ui->edits[2], ui->submit};
    for (i = 0; i < 3; ++i)
    {
        lv_area_t area;
        int j;
        int offset;
        lv_group_focus_obj(controls[i]);
        lv_refr_now(ui->display);
        if (i == 0)
        {
            ck_assert_int_eq(lv_obj_get_scroll_x(controls[i]), 0);
        }
        lv_obj_get_coords(controls[i], &area);
        offset = lv_obj_get_style_outline_pad(controls[i], 0) + 1;
        lv_point_t points[] =
        {
            {area.x1 - offset, (area.y1 + area.y2) / 2},
            {area.x2 + offset, (area.y1 + area.y2) / 2},
            {(area.x1 + area.x2) / 2, area.y2 + offset}
        };
        for (j = 0; j < 3; ++j)
        {
            ck_assert_int_ge(points[j].x, 0);
            ck_assert_int_lt(points[j].x, ui->frame->width);
            ck_assert_int_ge(points[j].y, 0);
            ck_assert_int_lt(points[j].y, ui->frame->height);
            const uint32_t *pixels = (const uint32_t *)(ui->frame->data +
                                     points[j].y * ui->frame->line_size);
            ck_assert_int_eq(pixels[points[j].x] & 0xffffff, 0x007aff);
        }
    }
    free_fixture(&f);
}
END_TEST

START_TEST(test_lvgl_gfx_handoff)
{
    struct login_fixture f;
    struct xrdp_rect rect = {0, 0, 800, 600};
    struct xrdp_region *desktop_damage;
    init_fixture(&f, 32);
    ck_assert_int_eq(xrdp_login_lvgl_create(&f.wm, 0), 0);
    f.client.gfx = 1;
    f.wm.screen_dirty_region = xrdp_region_create(&f.wm);
    xrdp_region_add_rect(f.wm.screen_dirty_region, &rect);
    f.mm.mod_uses_wm_screen_for_gfx = 1;
    xrdp_login_lvgl_prepare_connect(&f.wm);
    ck_assert_ptr_null(f.wm.screen_dirty_region);
    ck_assert_int_eq(f.mm.mod_uses_wm_screen_for_gfx, 0);
    ck_assert_ptr_nonnull(f.wm.login_ui); /* A failed connect can still show errors. */

    /* A module may queue its first frame while connecting. Preserve it, but
     * prevent a pending toolkit refresh from painting over it afterwards. */
    desktop_damage = xrdp_region_create(&f.wm);
    xrdp_region_add_rect(desktop_damage, &rect);
    f.wm.screen_dirty_region = desktop_damage;
    f.wm.login_ui->dirty = 1;
    xrdp_wm_mod_connect_done(&f.wm, 0);
    ck_assert_int_eq(f.wm.login_state, WMLS_CLEANUP);
    ck_assert_ptr_null(f.wm.login_ui);
    ck_assert_int_eq(xrdp_login_lvgl_check(&f.wm), 0);
    ck_assert_ptr_eq(f.wm.screen_dirty_region, desktop_damage);
    ck_assert_int_eq(xrdp_region_get_rect(desktop_damage, 0, &rect), 0);
    xrdp_region_delete(desktop_damage);
    f.wm.screen_dirty_region = NULL;
    free_fixture(&f);
}
END_TEST

START_TEST(test_lvgl_pixels_and_isolation)
{
    struct login_fixture f;
    struct login_fixture other[4];
    unsigned int j;
    struct xrdp_login_lvgl *ui;
    const int depths[] = {15, 16, 24, 32};
    unsigned int d;
    for (j = 0; j < 4; ++j)
    {
        init_fixture(&other[j], 24);
        if (j == 3)
        {
            other[j].config.cfg_globals.default_dpi = 192;
            other[j].client.display_sizes.monitorCount = 2;
            other[j].client.display_sizes.minfo_wm[1].is_primary = 1;
            other[j].client.display_sizes.minfo_wm[1].left = 400;
            other[j].client.display_sizes.minfo_wm[1].right = 799;
            other[j].client.display_sizes.minfo_wm[1].bottom = 599;
        }
        ck_assert_int_eq(xrdp_login_lvgl_create(&other[j].wm, 1), 0);
        if (j == 3)
        {
            ck_assert_int_eq(other[j].wm.login_ui->dpi, 192);
            ck_assert_int_ge(lv_obj_get_x(other[j].wm.login_ui->card), 400);
        }
        lv_refr_now(other[j].wm.login_ui->display);
        other[j].wm.login_ui->dirty = 0;
    }
    for (d = 0; d < sizeof(depths) / sizeof(depths[0]); ++d)
    {
        lv_area_t area = {-1, -1, 1, 1};
        uint32_t stride = lv_draw_buf_width_to_stride(3, LV_COLOR_FORMAT_XRGB8888);
        uint8_t *pixels = calloc(3, stride);
        int x, y;
        init_fixture(&f, depths[d]);
        ck_assert_int_eq(xrdp_login_lvgl_create(&f.wm, 1), 0);
        ui = f.wm.login_ui;
        ck_assert_ptr_ne(ui->display, other[0].wm.login_ui->display);
        ck_assert_ptr_ne(ui->group, other[0].wm.login_ui->group);
        ck_assert_int_eq(ui_users, 5);
        for (y = 0; y < 3; ++y)
            for (x = 0; x < 3; ++x)
            {
                ((uint32_t *)(pixels + y * stride))[x] = 0x00ff8000;
            }
        flush_pixels(ui->display, &area, pixels);
        ck_assert_int_eq(ui->damage.left, 0);
        ck_assert_int_eq(ui->damage.top, 0);
        ck_assert_int_eq(ui->damage.right, 2);
        ck_assert_int_eq(ui->damage.bottom, 2);
        if (depths[d] == 15)
        {
            ck_assert_int_eq(((uint16_t *)ui->frame->data)[0], COLOR15(255, 128, 0));
        }
        else if (depths[d] == 16)
        {
            ck_assert_int_eq(((uint16_t *)ui->frame->data)[0], COLOR16(255, 128, 0));
        }
        else
        {
            ck_assert_int_eq(((uint32_t *)ui->frame->data)[0], 0xff8000);
        }
        ck_assert_int_eq(other[0].wm.login_ui->dirty, 0);
        free(pixels);
        free_fixture(&f);
        ck_assert_int_eq(ui_users, 4);
    }
    for (j = 0; j < 4; ++j)
    {
        free_fixture(&other[j]);
    }
    ck_assert_int_eq(ui_users, 0);
}
END_TEST

static void *
concurrent_login(void *arg)
{
    int i;
    for (i = 0; i < 12; ++i)
    {
        struct login_fixture f;
        init_fixture(&f, 24);
        ck_assert_int_eq(xrdp_login_lvgl_create(&f.wm, 1), 0);
        xrdp_login_lvgl_key(&f.wm, 0, (unsigned int)(intptr_t)arg, 1, 0);
        pthread_mutex_lock(&ui_mutex);
        ck_assert_int_eq(lv_textarea_get_text(f.wm.login_ui->edits[1])[0], (intptr_t)arg);
        lv_refr_now(f.wm.login_ui->display);
        pthread_mutex_unlock(&ui_mutex);
        free_fixture(&f);
    }
    return NULL;
}

START_TEST(test_lvgl_concurrent_lifecycle)
{
    pthread_t first;
    pthread_t second;
    ck_assert_int_eq(pthread_create(&first, NULL, concurrent_login, (void *)(intptr_t)'a'), 0);
    ck_assert_int_eq(pthread_create(&second, NULL, concurrent_login, (void *)(intptr_t)'b'), 0);
    pthread_join(first, NULL);
    pthread_join(second, NULL);
    ck_assert_int_eq(ui_users, 0);
}
END_TEST

START_TEST(test_lvgl_input_and_lifecycle)
{
    struct login_fixture f;
    struct xrdp_login_lvgl *ui;
    lv_obj_t *edit;
    char text[256];
    const char *screenshot;
    init_fixture(&f, 24);
    ck_assert_int_eq(xrdp_login_lvgl_create(&f.wm, 1), 0);
    ui = f.wm.login_ui;
    edit = ui->edits[1]; /* name=Desktop precedes username */
    ck_assert_ptr_nonnull(edit);
    lv_group_focus_obj(edit);
    xrdp_login_lvgl_key(&f.wm, 0, 0xe9, 1, 0);
    ck_assert_str_eq(lv_textarea_get_text(edit), "é");
    xrdp_login_lvgl_key(&f.wm, 0, 0xe9, 0, 0);
    ck_assert_str_eq(lv_textarea_get_text(edit), "é");
    xrdp_login_lvgl_key(&f.wm, 0xff08, 0, 1, 0);
    ck_assert_str_eq(lv_textarea_get_text(edit), "");
    memset(text, 'a', 254);
    text[254] = '\0';
    lv_textarea_set_text(edit, text);
    xrdp_login_lvgl_key(&f.wm, 0, 0xe9, 1, 0); /* Would exceed 255 bytes */
    ck_assert_int_eq(strlen(lv_textarea_get_text(edit)), 254);
    xrdp_login_lvgl_key(&f.wm, 0, 'b', 1, 0);
    ck_assert_int_eq(strlen(lv_textarea_get_text(edit)), 255);
    xrdp_login_lvgl_key(&f.wm, 0xff08, 0, 1, 0);
    ck_assert_int_eq(strlen(lv_textarea_get_text(edit)), 254);
    lv_textarea_set_text(edit, "alice");
    xrdp_login_lvgl_key(&f.wm, 0, '\t', 1, 0);
    ck_assert(lv_textarea_get_password_mode(lv_group_get_focused(ui->group)));
    ck_assert_int_eq(lv_textarea_get_password_show_time(lv_group_get_focused(ui->group)), 0);
    xrdp_login_lvgl_key(&f.wm, 0, 's', 1, 0);
    handle_action(&f.wm, UI_HELP);
    ck_assert_ptr_nonnull(ui->help);
    handle_action(&f.wm, UI_CLOSE_HELP);
    ck_assert_ptr_null(ui->help);
    lv_dropdown_set_selected(ui->sessions, 1);
    handle_action(&f.wm, UI_SELECT);
    ck_assert_int_eq(ui->selected, 1);
    ck_assert_str_eq(lv_textarea_get_text(ui->edits[0]), "alice");
    ck_assert_str_eq(lv_textarea_get_text(ui->edits[1]), "secret");
    ck_assert(lv_textarea_get_password_mode(ui->edits[1]));
    lv_dropdown_set_selected(ui->sessions, 0);
    handle_action(&f.wm, UI_SELECT);
    edit = ui->edits[1];
    lv_textarea_set_text(edit, "alice");
    ui->dirty = 0;
    timer_due = g_get_elapsed_ms() + 1000;
    ck_assert_int_eq(xrdp_bitmap_resize(f.wm.screen, 640, 480), 0);
    ck_assert_int_eq(xrdp_login_lvgl_check(&f.wm), 0);
    ck_assert_ptr_eq(ui->edits[1], edit);
    ck_assert_str_eq(lv_textarea_get_text(edit), "alice");
    ck_assert_int_eq(ui->frame->width, 640);
    ck_assert_int_eq(ui->frame->height, 480);
    lv_obj_scroll_to_y(ui->card, 0, LV_ANIM_OFF);
    ui->mouse.x = 100;
    ui->mouse.y = 100;
    xrdp_wm_mouse_click(&f.wm, 0, 0, 5, 0);
    ck_assert_int_gt(lv_obj_get_scroll_y(ui->card), 0);
    ck_assert_int_eq(ui->mouse.x, 100);
    lv_refr_now(ui->display);
    ck_assert(ui->dirty);
    /* Drain deferred focus/layout damage after resizing. */
    lv_refr_now(ui->display);
    ui->dirty = 0;
    lv_refr_now(ui->display);
    ck_assert_int_eq(ui->dirty, 0); /* An unchanged view sends no pixels. */
    lv_group_focus_obj(edit);
    xrdp_login_lvgl_key(&f.wm, 0, 'x', 1, 0);
    lv_refr_now(ui->display);
    ck_assert(ui->dirty);
    ck_assert_int_lt(ui->damage.right - ui->damage.left, ui->frame->width);
    screenshot = getenv("XRDP_LVGL_SCREENSHOT");
    if (screenshot != NULL)
    {
        g_save_to_bmp(screenshot, ui->frame->data, ui->frame->line_size,
                      ui->frame->width, ui->frame->height, 24, 32);
    }
    handle_action(&f.wm, UI_SUBMIT);
    ck_assert_int_eq(f.wm.login_state, WMLS_START_CONNECT);
    ck_assert_int_eq(ui->edit_count, 0);
    xrdp_login_lvgl_progress(&f.wm);
    ck_assert_int_eq(log_mark_for(LOG_LEVEL_INFO, "sesman connect ok"), LOG_MARK_SUCCESS);
    ck_assert_int_eq(log_mark_for(LOG_LEVEL_INFO, "Connecting to display server"), LOG_MARK_PROGRESS);
    ck_assert_int_eq(log_mark_for(LOG_LEVEL_WARNING, "Obsolete parameter"), LOG_MARK_WARNING);
    ck_assert_int_eq(log_mark_for(LOG_LEVEL_ERROR, "sesman connect ok"), LOG_MARK_ERROR);
    timer_due = g_get_elapsed_ms() + 60000; /* Simulate an idle toolkit. */
    xrdp_wm_log_msg(&f.wm, LOG_LEVEL_INFO, "sesman connect ok");
    ck_assert_int_le((int)(timer_due - g_get_elapsed_ms()), 0);
    xrdp_wm_log_msg(&f.wm, LOG_LEVEL_DEBUG, "Sending login information");
    xrdp_wm_log_msg(&f.wm, LOG_LEVEL_ERROR, "Authentication failed");
    ck_assert_int_eq(lv_obj_get_child_count(ui->log_rows), 3);
    lv_obj_t *first_row = lv_obj_get_child(ui->log_rows, 0);
    xrdp_login_lvgl_log(&f.wm, 1);
    ck_assert_int_eq(ui->mode, 2);
    ck_assert_ptr_eq(lv_obj_get_child(ui->log_rows, 0), first_row);
    ck_assert_str_eq(lv_label_get_text(lv_obj_get_child(lv_obj_get_child(ui->log_rows, 2), 1)), "Authentication failed");
    lv_refr_now(ui->display); /* Exercise the font-independent status markers. */
    {
        lv_area_t area;
        int x, y, dot_pixels = 0;
        lv_obj_get_coords(lv_obj_get_child(lv_obj_get_child(ui->log_rows, 1), 0), &area);
        for (y = MAX(0, area.y1); y <= MIN(ui->frame->height - 1, area.y2); ++y)
        {
            const uint32_t *pixels = (const uint32_t *)(ui->frame->data + y * ui->frame->line_size);
            for (x = MAX(0, area.x1); x <= MIN(ui->frame->width - 1, area.x2); ++x)
            {
                dot_pixels += (pixels[x] & 0xffffff) == 0x64748b;
            }
        }
        ck_assert_int_gt(dot_pixels, 0);
    }
    screenshot = getenv("XRDP_LVGL_LOG_SCREENSHOT");
    if (screenshot != NULL)
    {
        g_save_to_bmp(screenshot, ui->frame->data, ui->frame->line_size,
                      ui->frame->width, ui->frame->height, 24, 32);
    }
    handle_action(&f.wm, UI_ACK);
    ck_assert_int_eq(f.wm.login_state, WMLS_RESET);
    free_fixture(&f);
    init_fixture(&f, 24);
    ck_assert_int_eq(xrdp_login_lvgl_create(&f.wm, 0), 0);
    f.client.rdp_autologin = 1;
    f.wm.hide_log_window = 1;
    list_add_strdup(f.wm.log, "Hidden connection detail");
    xrdp_login_lvgl_log_message(&f.wm, LOG_LEVEL_ERROR, "Hidden connection detail");
    ck_assert_ptr_null(f.wm.login_ui->log_rows);
    ck_assert_str_eq(lv_label_get_text(f.wm.login_ui->status), "Connecting…");
    xrdp_wm_show_log(&f.wm);
    ck_assert_int_eq(f.client.rdp_autologin, 0);
    ck_assert_int_eq(f.wm.login_state, WMLS_RESET);
    ck_assert_int_eq(f.wm.login_ui->mode, 1);
    free_fixture(&f);
    init_fixture(&f, 24);
    ck_assert_int_eq(xrdp_login_lvgl_create(&f.wm, 1), 0);
    handle_action(&f.wm, UI_HELP);
    f.wm.fatal_error_in_log_window = 1;
    xrdp_wm_show_log(&f.wm);
    ck_assert_ptr_null(f.wm.login_ui->help);
    ck_assert(!lv_obj_has_flag(f.wm.login_ui->card, LV_OBJ_FLAG_HIDDEN));
    handle_action(&f.wm, UI_ACK);
    ck_assert_int_eq(f.wm.login_state, WMLS_RESET);
    free_fixture(&f);
    init_fixture(&f, 24);
    strcpy(f.config.cfg_globals.ls_font_file, "/no/such/font.ttf");
    ck_assert_int_eq(xrdp_login_lvgl_create(&f.wm, 1), 1);
    ck_assert_ptr_null(f.wm.login_ui);
    ck_assert_int_eq(ui_users, 0);
    free_fixture(&f);
    init_fixture(&f, 8);
    ck_assert_int_eq(xrdp_login_lvgl_create(&f.wm, 1), 1);
    free_fixture(&f);
}
END_TEST
#endif

Suite *
make_suite_login(void)
{
    Suite *suite = suite_create("login");
    TCase *tc = tcase_create("login");
    tcase_set_timeout(tc, 20);
    tcase_add_test(tc, test_login_model);
#ifdef XRDP_LVGL
    tcase_add_test(tc, test_lvgl_focus_outline);
    tcase_add_test(tc, test_lvgl_logo_alpha);
    tcase_add_test(tc, test_lvgl_gfx_handoff);
    tcase_add_test(tc, test_lvgl_pixels_and_isolation);
    tcase_add_test(tc, test_lvgl_input_and_lifecycle);
    tcase_add_test(tc, test_lvgl_concurrent_lifecycle);
#endif
    suite_add_tcase(suite, tc);
    return suite;
}
