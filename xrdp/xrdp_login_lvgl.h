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
 * Optional pre-session UI. No LVGL types escape the adapter.
 */

#ifndef XRDP_LOGIN_LVGL_H
#define XRDP_LOGIN_LVGL_H

struct xrdp_wm;
struct xrdp_rect;
int xrdp_login_lvgl_create(struct xrdp_wm *wm, int prompt);
void xrdp_login_lvgl_delete(struct xrdp_wm *wm);
void xrdp_login_lvgl_prepare_connect(struct xrdp_wm *wm);
void xrdp_login_lvgl_progress(struct xrdp_wm *wm);
void xrdp_login_lvgl_log(struct xrdp_wm *wm, int error);
void xrdp_login_lvgl_log_message(struct xrdp_wm *wm, int level, const char *message);
void xrdp_login_lvgl_invalidate(struct xrdp_wm *wm, const struct xrdp_rect *rect);
void xrdp_login_lvgl_mouse(struct xrdp_wm *wm, int x, int y, int button, int down);
void xrdp_login_lvgl_key(struct xrdp_wm *wm, int sym, unsigned int chr, int down, int shift);
void xrdp_login_lvgl_wait(struct xrdp_wm *wm, tbus *objs, int *count, int *timeout);
int xrdp_login_lvgl_check(struct xrdp_wm *wm);
#endif
