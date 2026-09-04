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
 * Recognises Ctrl+V and Shift+Insert. V is matched on the keysym, in
 * both the lowercase and uppercase (Caps Lock / Shift) foldings, so the
 * binding is correct on non-QWERTY Latin layouts such as Dvorak; it is
 * also matched on the physical key as a fallback, so layouts that never
 * produce a Latin V - Cyrillic, Greek, Hebrew, Arabic, Thai, and so on -
 * can still paste.
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

/**
 * Parse a CB_FORMAT_LIST PDU body for the text formats we can use
 *
 * @param s Stream positioned at the start of the PDU body
 * @param msg_flags msgFlags from the cliprdr header
 * @param use_long_names Non-zero if the client negotiated long format names
 * @param have_unicode_text Set to 1 if CF_UNICODETEXT is offered
 * @param have_text Set to 1 if CF_TEXT is offered
 * @return 0 on success, 1 if the PDU is malformed
 *
 * See [MS-RDPECLIP] 2.2.3.1. Format names are skipped, but must be
 * skipped by the right width for the negotiated framing.
 */
int
xrdp_login_clip_parse_format_list(struct stream *s, int msg_flags,
                                  int use_long_names,
                                  int *have_unicode_text, int *have_text);

/**
 * Create the login screen clipboard handler and start the handshake
 *
 * @param wm Window manager, used for sending and for the target widget
 * @param chan_id cliprdr static channel id, from libxrdp_get_channel_id()
 * @return New object, or NULL on failure
 *
 * The caller is responsible for checking that the feature is enabled and
 * that the channel exists. Creating this object sends CB_CLIP_CAPS and
 * CB_MONITOR_READY to the client.
 */
struct xrdp_login_clip *
xrdp_login_clip_create(struct xrdp_wm *wm, int chan_id);

/**
 * Destroy the handler, clearing any buffered clipboard content
 *
 * @param self Object to free, may be NULL
 */
void
xrdp_login_clip_delete(struct xrdp_login_clip *self);

/**
 * Process cliprdr channel data from the client
 *
 * @param self Object, may be NULL
 * @param flags Channel PDU flags, XR_CHANNEL_FLAG_*
 * @param data Chunk data
 * @param len Length of this chunk
 * @param total_len Length of the whole PDU
 * @return Always 0 - protocol failures are absorbed, never propagated
 */
int
xrdp_login_clip_process_channel_data(struct xrdp_login_clip *self,
                                     int flags, const char *data,
                                     int len, int total_len);

/**
 * Ask the client for its clipboard text
 *
 * @param self Object, may be NULL
 * @return 1 if a request was sent, 0 otherwise
 *
 * Called only from a paste keypress. This is the only path that asks the
 * client for clipboard content.
 */
int
xrdp_login_clip_request_paste(struct xrdp_login_clip *self);

#endif /* XRDP_LOGIN_CLIP_H */
