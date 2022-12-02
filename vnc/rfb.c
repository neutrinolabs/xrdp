/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2015
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
 * libvnc - RFB specific functions
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "rfb.h"

/* Messages for ExtendedDesktopSize status code */
static const char *eds_status_msg[] =
{
    /* 0 */ "No error",
    /* 1 */ "Resize is administratively prohibited",
    /* 2 */ "Out of resources",
    /* 3 */ "Invalid screen layout",
    /* others */ "Unknown code"
};

/* elements in above list */
#define EDS_STATUS_MSG_COUNT \
    (sizeof(eds_status_msg) / sizeof(eds_status_msg[0]))

/**************************************************************************//**
 * Returns an error string for an ExtendedDesktopSize status code
 */
const char *
rfb_get_eds_status_msg(unsigned int response_code)
{
    if (response_code >= EDS_STATUS_MSG_COUNT)
    {
        response_code = EDS_STATUS_MSG_COUNT - 1;
    }

    return eds_status_msg[response_code];
}
