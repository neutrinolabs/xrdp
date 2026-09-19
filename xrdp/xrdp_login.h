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
 * Shared data operations for the legacy and LVGL login screens.
 */

#ifndef XRDP_LOGIN_H
#define XRDP_LOGIN_H

struct xrdp_wm;
struct xrdp_mod_data;
struct list;
int xrdp_login_load_modules(struct xrdp_wm *, struct list *, struct list *);
int xrdp_login_parse_domain(char *, int, int, char *, unsigned int);
int xrdp_login_get_field(struct xrdp_wm *, struct xrdp_mod_data *, int, int, char [256]);
int xrdp_login_set_value(struct xrdp_mod_data *, const char *, const char *);
int xrdp_login_is_secret(const char *);
void xrdp_login_submit(struct xrdp_wm *, struct xrdp_mod_data *);
void xrdp_login_free_modules(struct list *);
#endif
