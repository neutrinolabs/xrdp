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
 *
 * External interface to the smartcard function
 */

#ifndef SMARTCARD_H
#define SMARTCARD_H

#include "arch.h"

void scard_device_announce(tui32 device_id);
int  scard_get_wait_objs(tbus *objs, int *count, int *timeout);
int  scard_check_wait_objs(void);
int  scard_init(void);
int  scard_deinit(void);

#endif /* end #ifndef SMARTCARD_H */
