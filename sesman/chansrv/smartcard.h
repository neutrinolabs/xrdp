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

/* forward declarations */
void scard_device_announce(tui32 device_id);

/* callbacks into this module */
void scard_handle_EstablishContext_Return(struct stream *s, IRP *irp,
                                          tui32 DeviceId, tui32 CompletionId,
                                          tui32 IoStatus);

void scard_handle_ListReaders_Return(struct stream *s, IRP *irp,
                                     tui32 DeviceId, tui32 CompletionId,
                                     tui32 IoStatus);

#endif /* end #ifndef _SMARTCARD_C */
