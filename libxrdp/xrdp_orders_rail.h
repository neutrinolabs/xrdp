/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2012-2014
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

#if !defined(_XRDP_ORDERS_RAIL_H)
#define _XRDP_ORDERS_RAIL_H

int
xrdp_orders_send_window_delete(struct xrdp_orders *self, int window_id);
int
xrdp_orders_send_window_cached_icon(struct xrdp_orders *self,
                                    int window_id, int cache_entry,
                                    int cache_id, int flags);
int
xrdp_orders_send_window_icon(struct xrdp_orders *self,
                             int window_id, int cache_entry, int cache_id,
                             struct rail_icon_info *icon_info,
                             int flags);
int
xrdp_orders_send_window_new_update(struct xrdp_orders *self, int window_id,
                                   struct rail_window_state_order *window_state,
                                   int flags);
int
xrdp_orders_send_notify_delete(struct xrdp_orders *self, int window_id,
                               int notify_id);
int
xrdp_orders_send_notify_new_update(struct xrdp_orders *self,
                                   int window_id, int notify_id,
                                   struct rail_notify_state_order *notify_state,
                                   int flags);
int
xrdp_orders_send_monitored_desktop(struct xrdp_orders *self,
                                   struct rail_monitored_desktop_order *mdo,
                                   int flags);

#endif
