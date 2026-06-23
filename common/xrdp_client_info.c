/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2024
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
 * @file common/xrdp_client_info.c
 * @brief Pure helpers derived from client display information
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <limits.h>

#include "xrdp_client_info.h"

/*****************************************************************************/
unsigned int
xrdp_client_info_calculate_dpi(unsigned int height_pixels,
                               unsigned int height_mm)
{
    unsigned int dpi = 0;

    /*
     * DPI = height_pixels / (height_mm / 25.4)
     *     = (height_pixels * 25.4) / height_mm
     *     = (height_pixels * 127) / (height_mm * 5)
     *
     * Reject a missing physical size or pixel height, and guard the
     * integer arithmetic against overflow before computing.
     */
    if (height_mm != 0 && height_pixels != 0 &&
            height_pixels <= UINT_MAX / 127u &&
            height_mm <= UINT_MAX / 5u)
    {
        dpi = (height_pixels * 127u) / (height_mm * 5u);
    }

    return dpi;
}

/*****************************************************************************/
int
xrdp_client_info_dpi_valid_for_session(unsigned int dpi)
{
    return (dpi >= XRDP_SESSION_DPI_MIN && dpi <= XRDP_SESSION_DPI_MAX);
}
