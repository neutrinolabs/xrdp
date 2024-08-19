/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Koichiro Iwao
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

/**
 *
 * @file xrdp_tconfig.c
 * @brief TOML config loader
 * @author Koichiro Iwao
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arch.h"
#include "os_calls.h"
#include "parse.h"
#include "toml.h"
#include "ms-rdpbcgr.h"
#include "xrdp_tconfig.h"
#include "string_calls.h"

#define TCLOG(log_level, args...) LOG(log_level, "TConfig: " args)

#define X264_DEFAULT_PRESET  "ultrafast"
#define X264_DEFAULT_TUNE    "zerolatency"
#define X264_DEFAULT_PROFILE "main"
#define X264_DEFAULT_FPS_NUM 24
#define X264_DEFAULT_FPS_DEN 1

static int
tconfig_load_gfx_x264_ct(toml_table_t *tfile, const int connection_type,
                         struct xrdp_tconfig_gfx_x264_param *param)
{
    if (connection_type > NUM_CONNECTION_TYPES)
    {
        TCLOG(LOG_LEVEL_ERROR, "Invalid connection type is given");
        return 1;
    }

    toml_table_t *x264 = toml_table_in(tfile, "x264");
    if (!x264)
    {
        TCLOG(LOG_LEVEL_WARNING, "x264 params are not defined");
        return 1;
    }

    toml_table_t *x264_ct =
        toml_table_in(x264, rdpbcgr_connection_type_names[connection_type]);
    toml_datum_t datum;

    if (!x264_ct)
    {
        TCLOG(LOG_LEVEL_WARNING, "x264 params for connection type [%s] is not defined",
              rdpbcgr_connection_type_names[connection_type]);
        return 1;
    }

    /* preset */
    datum = toml_string_in(x264_ct, "preset");
    if (datum.ok)
    {
        g_strncpy(param[connection_type].preset,
                  datum.u.s,
                  sizeof(param[connection_type].preset) - 1);
        free(datum.u.s);
    }
    else if (connection_type == 0)
    {
        TCLOG(LOG_LEVEL_WARNING,
              "x264 param preset is not set for connection type [%s], "
              "adopting the default value \"" X264_DEFAULT_PRESET "\"",
              rdpbcgr_connection_type_names[connection_type]);
        g_strncpy(param[connection_type].preset,
                  X264_DEFAULT_PRESET,
                  sizeof(param[connection_type].preset) - 1);
    }

    /* tune */
    datum = toml_string_in(x264_ct, "tune");
    if (datum.ok)
    {
        g_strncpy(param[connection_type].tune,
                  datum.u.s,
                  sizeof(param[connection_type].tune) - 1);
        free(datum.u.s);
    }
    else if (connection_type == 0)
    {
        TCLOG(LOG_LEVEL_WARNING,
              "x264 param tune is not set for connection type [%s], "
              "adopting the default value \"" X264_DEFAULT_TUNE "\"",
              rdpbcgr_connection_type_names[connection_type]);
        g_strncpy(param[connection_type].tune,
                  X264_DEFAULT_TUNE,
                  sizeof(param[connection_type].tune) - 1);
    }

    /* profile */
    datum = toml_string_in(x264_ct, "profile");
    if (datum.ok)
    {
        g_strncpy(param[connection_type].profile,
                  datum.u.s,
                  sizeof(param[connection_type].profile) - 1);
        free(datum.u.s);
    }
    else if (connection_type == 0)
    {
        TCLOG(LOG_LEVEL_WARNING,
              "x264 param profile is not set for connection type [%s], "
              "adopting the default value \"" X264_DEFAULT_PROFILE "\"",
              rdpbcgr_connection_type_names[connection_type]);
        g_strncpy(param[connection_type].profile,
                  X264_DEFAULT_PROFILE,
                  sizeof(param[connection_type].profile) - 1);
    }

    /* vbv_max_bitrate */
    datum = toml_int_in(x264_ct, "vbv_max_bitrate");
    if (datum.ok)
    {
        param[connection_type].vbv_max_bitrate = datum.u.i;
    }
    else if (connection_type == 0)
    {
        TCLOG(LOG_LEVEL_WARNING,
              "x264 param vbv_max_bit_rate is not set for connection type [%s], "
              "adopting the default value [0]",
              rdpbcgr_connection_type_names[connection_type]);
        param[connection_type].vbv_max_bitrate = 0;
    }

    /* vbv_buffer_size */
    datum = toml_int_in(x264_ct, "vbv_buffer_size");
    if (datum.ok)
    {
        param[connection_type].vbv_buffer_size = datum.u.i;
    }
    else if (connection_type == 0)
    {
        TCLOG(LOG_LEVEL_WARNING,
              "x264 param vbv_buffer_size is not set for connection type [%s], "
              "adopting the default value [0]",
              rdpbcgr_connection_type_names[connection_type]);
        param[connection_type].vbv_buffer_size = 0;
    }

    /* fps_num */
    datum = toml_int_in(x264_ct, "fps_num");
    if (datum.ok)
    {
        param[connection_type].fps_num = datum.u.i;
    }
    else if (connection_type == 0)
    {
        TCLOG(LOG_LEVEL_WARNING,
              "x264 param fps_num is not set for connection type [%s], "
              "adopting the default value [0]",
              rdpbcgr_connection_type_names[connection_type]);
        param[connection_type].fps_num = X264_DEFAULT_FPS_NUM;
    }

    /* fps_den */
    datum = toml_int_in(x264_ct, "fps_den");
    if (datum.ok)
    {
        param[connection_type].fps_den = datum.u.i;
    }
    else if (connection_type == 0)
    {
        TCLOG(LOG_LEVEL_WARNING,
              "x264 param fps_den is not set for connection type [%s], "
              "adopting the default value [0]",
              rdpbcgr_connection_type_names[connection_type]);
        param[connection_type].fps_num = X264_DEFAULT_FPS_DEN;
    }

    return 0;
}

int
tconfig_load_gfx(const char *filename, struct xrdp_tconfig_gfx *config)
{
    FILE *fp;
    char errbuf[200];
    toml_table_t *tfile;

    if ((fp = fopen(filename, "r")) == NULL)
    {
        TCLOG(LOG_LEVEL_ERROR, "Error loading GFX config file %s (%s)",
              filename, g_get_strerror());
        return 1;
    }
    else if ((tfile = toml_parse_file(fp, errbuf, sizeof(errbuf))) == NULL)
    {
        TCLOG(LOG_LEVEL_ERROR, "Error in GFX config file %s - %s", filename, errbuf);
        fclose(fp);
        return 1;
    }
    else
    {
        TCLOG(LOG_LEVEL_INFO, "Loading GFX config file %s", filename);
        fclose(fp);

        memset(config, 0, sizeof(struct xrdp_tconfig_gfx));

        /* First of all, read the default params and override later */
        tconfig_load_gfx_x264_ct(tfile, 0, config->x264_param);

        for (int ct = CONNECTION_TYPE_MODEM; ct < NUM_CONNECTION_TYPES; ct++)
        {
            memcpy(&config->x264_param[ct], &config->x264_param[0],
                   sizeof(struct xrdp_tconfig_gfx_x264_param));

            tconfig_load_gfx_x264_ct(tfile, ct, config->x264_param);
        }

        toml_free(tfile);

        return 0;
    }
}

