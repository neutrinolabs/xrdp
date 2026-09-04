/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2026
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
 * Clipboard paste support for the xrdp login screen
 *
 * This module implements the receiving half of [MS-RDPECLIP] inside the
 * xrdp process, for use before a session exists. It is receive-only: it
 * never advertises server clipboard formats, and only ever requests data
 * from the client in direct response to a paste keypress.
 */

#ifndef XRDP_LOGIN_CLIP_H
#define XRDP_LOGIN_CLIP_H

#include "arch.h"

struct xrdp_wm;
struct xrdp_bitmap;
struct xrdp_key_info;
struct stream;
struct xrdp_login_clip;

/**
 * Is this keypress a paste request?
 *
 * @param keys Key state array, indexed by scancode_to_index()
 * @param scan_code Scancode from SCANCODE_FROM_KBD_EVENT()
 * @param ki Key info for the keypress, may be NULL
 * @return 1 if the keypress means "paste", 0 otherwise
 *
 * Recognises Ctrl+V and Shift+Insert. V is matched on the keysym rather
 * than the scancode so the binding is correct on non-QWERTY layouts.
 *
 * AltGr is excluded deliberately: Windows clients send AltGr as
 * LCTRL-down plus RALT-down, so without that check AltGr+V would look
 * like a paste on layouts where it is a real character.
 */
int
xrdp_login_clip_is_paste_key(const int *keys, int scan_code,
                             const struct xrdp_key_info *ki);

/**
 * Decode UTF-16LE clipboard text into sanitised Unicode codepoints
 *
 * @param utf16le UTF-16LE text from the client
 * @param bytes Length of utf16le in bytes
 * @param out Where to write codepoints
 * @param out_count Capacity of out, in codepoints
 * @return Number of codepoints written
 *
 * Decoding stops at a NUL word or at the first CR or LF, so a trailing
 * newline is harmless and a multi-line clipboard cannot spill across
 * fields. Control characters are dropped, surrogate pairs are combined,
 * and unpaired surrogates are discarded.
 */
unsigned int
xrdp_login_clip_utf16_to_codepoints(const char *utf16le, unsigned int bytes,
                                    char32_t *out, unsigned int out_count);

/**
 * Insert codepoints into an edit widget at its current caret position
 *
 * @param edit Edit widget, must be of type WND_TYPE_EDIT
 * @param cp Codepoints to insert
 * @param count Number of codepoints
 * @return Number of codepoints actually inserted
 *
 * Insertion stops silently when the widget's 256-byte caption fills up.
 * The caller is responsible for calling xrdp_bitmap_invalidate() once
 * afterwards if the return value is non-zero.
 */
unsigned int
xrdp_login_clip_insert_codepoints(struct xrdp_bitmap *edit,
                                  const char32_t *cp, unsigned int count);

#endif /* XRDP_LOGIN_CLIP_H */
