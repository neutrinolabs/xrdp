/**
 * xrdp: A Remote Desktop Protocol server.
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
 * This file contains the chansrv configuration parameters from sesman.ini
 */

#ifndef SESMAN_CLIP_RESTRICT_H
#define SESMAN_CLIP_RESTRICT_H

#define CLIP_RESTRICT_NONE 0
#define CLIP_RESTRICT_TEXT (1<<0)
#define CLIP_RESTRICT_FILE (1<<1)
#define CLIP_RESTRICT_IMAGE (1<<2)
#define CLIP_RESTRICT_ALL 0x7fffffff

/**
 * Converts a sesman clip restrict string to a bitmask
 *
 * @param inputstr Input string
 * @param unrecognised buffer for unrecognised tokens
 * @param unrecognised_len Length of the above
 * @return Bitmask
 *
 * The input is a comma-separated list of tokens as documented in
 * the sesman.ini manpage */
int
sesman_clip_restrict_string_to_bitmask(
    const char *inputstr,
    char *unrecognised,
    unsigned int unrecognised_len);

/**
 * Converts a sesman clip restrict bitmask to a string
 *
 * @param mask Input mask
 * @param output buffer for output string
 * @param output_len Length of the above
 * @return Length as for snprintf()
 */
int
sesman_clip_restrict_mask_to_string(
    int mask,
    char output[],
    unsigned int output_len);

#endif /* SESMAN_CLIP_RESTRICT_H */

