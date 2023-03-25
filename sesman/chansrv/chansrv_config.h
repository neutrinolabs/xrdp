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

#ifndef _CHANSRV_CONFIG
#define _CHANSRV_CONFIG

#include <sys/stat.h>

struct config_chansrv
{
    /** Whether the FUSE mount is enabled or not */
    int enable_fuse_mount;

    /** RestrictOutboundClipboard setting from sesman.ini */
    int restrict_outbound_clipboard;
    /** RestrictInboundClipboard setting from sesman.ini */
    int restrict_inbound_clipboard;

    /** * FuseMountName from sesman.ini */
    char *fuse_mount_name;
    /** FileUmask from sesman.ini */
    mode_t file_umask;

    /** Whether to use nautilus3-compatible file lists for the clipboard */
    int use_nautilus3_flist_format;

    /** Number of silent frames to send before SNDC_CLOSE is sent, setting from sesman.ini */
    unsigned int num_silent_frames_aac;
    unsigned int num_silent_frames_mp3;
    /** Do net send sound data afer SNDC_CLOSE is sent. unit is millisecond, setting from sesman.ini */
    unsigned int msec_do_not_send;
};


/**
 *
 * @brief Reads sesman configuration
 * @param use_logger Use logger to log errors (otherwise stdout)
 * @param sesman_ini Name of configuration file to read
 *
 * @return configuration on success, NULL on failure
 *
 * @pre logging is assumed to be active
 * @post pass return value to config_free() to prevent memory leaks
 *
 */
struct config_chansrv *
config_read(int use_logger, const char *sesman_ini);

/**
 *
 * @brief Dumps configuration to stdout
 * @param pointer to a config_chansrv struct
 *
 */
void
config_dump(struct config_chansrv *config);

/**
 *
 * @brief Frees configuration allocated by config_read()
 * @param pointer to a config_chansrv struct (may be NULL)
 *
 */
void
config_free(struct config_chansrv *cs);

#endif /* _CHANSRV_CONFIG */
