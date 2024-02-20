/**
 * FreeRDP: A Remote Desktop Protocol Server
 * freerdp wrapper
 *
 * Copyright 2011-2013 Jay Sorg
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

#include <freerdp/freerdp.h>
#include <freerdp/codec/bitmap.h>

// FreeRDP defines some macros that we have different values for.
// To catch this we need to include the freerdp includes before our
// local ones (see gcc bug #16358)
#undef WM_LBUTTONUP
#undef WM_LBUTTONDOWN
#undef WM_RBUTTONUP
#undef WM_RBUTTONDOWN

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "xrdp-neutrinordp.h"
#include "xrdp-color.h"
#include "xrdp_rail.h"
#include "trans.h"
#include "log.h"
#include "os_calls.h"
#include "string_calls.h"

#if defined(VERSION_STRUCT_RDP_FREERDP)
#if VERSION_STRUCT_RDP_FREERDP > 1
#define NEUTRINORDP_HAS_SUPPRESS_OUTPUT
#endif
#endif

/* Max amount of buffered output data before we stop generating more */
#define MAX_QUEUED_MODULE_OUTPUT_DATA 50000


struct mod_context
{
    rdpContext _p;
    struct mod *modi;
};
typedef struct mod_context modContext;

/*****************************************************************************/
static void
verifyColorMap(struct mod *mod)
{
    int i;

    for (i = 0; i < 255; i++)
    {
        if (mod->colormap[i] != 0)
        {
            return ;
        }
    }

    LOG(LOG_LEVEL_WARNING, "The colormap is all NULL");
}

/*****************************************************************************/
static int
get_queued_module_output_data(struct mod *mod)
{
    return (mod->si != NULL) ? mod->si->source[XRDP_SOURCE_MOD] : 0;
}

/*****************************************************************************/
/* return error */
static int
lxrdp_start(struct mod *mod, int w, int h, int bpp)
{
    rdpSettings *settings;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "lxrdp_start: w %d h %d bpp %d", w, h, bpp);
    settings = mod->inst->settings;
    settings->width = w;
    settings->height = h;
    settings->color_depth = bpp;
    mod->bpp = bpp;

    settings->encryption = 1;
    settings->tls_security = 1;
    settings->nla_security = 0;
    settings->rdp_security = 1;

    return 0;
}

/******************************************************************************/
/* return error */
static void
set_keyboard_overrides(struct mod *mod)
{
    const struct kbd_overrides *ko = &mod->kbd_overrides;
    rdpSettings *settings = mod->inst->settings;

    if (mod->allow_client_kbd_settings)
    {
        settings->kbd_type = mod->client_info.keyboard_type;
        settings->kbd_subtype = mod->client_info.keyboard_subtype;
        /* Define the most common number of function keys, 12.
           because we can't get it from client. */
        settings->kbd_fn_keys = 12;
        settings->kbd_layout = mod->client_info.keylayout;

        /* Exception processing for each RDP Keyboard type */
        if (mod->client_info.keyboard_type == 0x00)
        {
            /* 0x00000000 : Set on Server */
            LOG(LOG_LEVEL_WARNING, "keyboard_type:[0x%02x] ,Set on Server",
                mod->client_info.keyboard_type);
        }
        else if (mod->client_info.keyboard_type == 0x04)
        {
            /* 0x00000004 : IBM enhanced (101- or 102-key) keyboard */
            /* Nothing to do. */
        }
        else if (mod->client_info.keyboard_type == 0x07)
        {
            /* 0x00000007 : Japanese keyboard */
            /* Nothing to do. */
        }
    }

    if (ko->type != 0)
    {
        LOG(LOG_LEVEL_INFO, "overrode kbd_type 0x%02X with 0x%02X",
            settings->kbd_type, ko->type);
        settings->kbd_type = ko->type;
    }

    if (ko->subtype != 0)
    {
        LOG(LOG_LEVEL_INFO, "overrode kbd_subtype 0x%02X with 0x%02X",
            settings->kbd_subtype, ko->subtype);
        settings->kbd_subtype = ko->subtype;
    }

    if (ko->fn_keys != 0)
    {
        LOG(LOG_LEVEL_INFO, "overrode kbd_fn_keys %d with %d",
            settings->kbd_fn_keys, ko->fn_keys);
        settings->kbd_fn_keys = ko->fn_keys;
    }

    if (ko->layout != 0)
    {
        LOG(LOG_LEVEL_INFO, "overrode kbd_layout 0x%08X with 0x%08X",
            settings->kbd_layout, ko->layout);
        settings->kbd_layout = ko->layout;
    }

    if (ko->layout_mask != 0)
    {
        LOG(LOG_LEVEL_INFO, "Masked kbd_layout 0x%08X to 0x%08X",
            settings->kbd_layout, settings->kbd_layout & ko->layout_mask);
        settings->kbd_layout &= ko->layout_mask;
    }

    LOG(LOG_LEVEL_INFO, "NeutrinoRDP proxy remote keyboard settings, "
        "kbd_type:[0x%02X], kbd_subtype:[0x%02X], "
        "kbd_fn_keys:[%02d], kbd_layout:[0x%08X]",
        settings->kbd_type, settings->kbd_subtype,
        settings->kbd_fn_keys, settings->kbd_layout);
}

static int
lxrdp_connect(struct mod *mod)
{
    boolean ok;
    set_keyboard_overrides(mod);

    LOG_DEVEL(LOG_LEVEL_TRACE, "lxrdp_connect:");

    ok = freerdp_connect(mod->inst);
    LOG_DEVEL(LOG_LEVEL_INFO, "lxrdp_connect: freerdp_connect returned %d", ok);

    if (!ok)
    {
        LOG_DEVEL(LOG_LEVEL_INFO, "Failure to connect");
#ifdef ERRORSTART

        if (connectErrorCode != 0)
        {
            char buf[128];

            if (connectErrorCode < ERRORSTART)
            {
                if (strerror_r(connectErrorCode, buf, 128) != 0)
                {
                    g_snprintf(buf, 128, "Errorcode from connect : %d", connectErrorCode);
                }
            }
            else
            {
                switch (connectErrorCode)
                {
                    case PREECONNECTERROR:
                        g_snprintf(buf, 128, "The error code from connect is "
                                   "PREECONNECTERROR");
                        break;

                    case UNDEFINEDCONNECTERROR:
                        g_snprintf(buf, 128, "The error code from connect is "
                                   "UNDEFINEDCONNECTERROR");
                        break;

                    case POSTCONNECTERROR:
                        g_snprintf(buf, 128, "The error code from connect is "
                                   "POSTCONNECTERROR");
                        break;

                    case DNSERROR:
                        g_snprintf(buf, 128, "The DNS system generated an error");
                        break;

                    case DNSNAMENOTFOUND:
                        g_snprintf(buf, 128, "The DNS system could not find the "
                                   "specified name");
                        break;

                    case CONNECTERROR:
                        g_snprintf(buf, 128, "A general connect error was returned");
                        break;

                    case MCSCONNECTINITIALERROR:
                        g_snprintf(buf, 128, "The error code from connect is "
                                   "MCSCONNECTINITIALERROR");
                        break;

                    case TLSCONNECTERROR:
                        g_snprintf(buf, 128, "Error in TLS handshake");
                        break;

                    case AUTHENTICATIONERROR:
                        g_snprintf(buf, 128, "Authentication error check your password "
                                   "and username");
                        break;

                    case INSUFFICIENTPRIVILEGESERROR:
                        g_snprintf(buf, 128, "Insufficient privileges on target server");
                        break;

                    default:
                        g_snprintf(buf, 128, "Unhandled Errorcode from connect : %d",
                                   connectErrorCode);
                        break;
                }
            }

            LOG(LOG_LEVEL_INFO, buf);
            mod->server_msg(mod, buf, 0);
        }
#else
        {
            /* This version of freerdp returns no useful information at
             * all */
            mod->server_msg(mod, "Neutrinordp connect failed.", 0);
            mod->server_msg(mod, "No more information is available", 0);
            mod->server_msg(mod, "Check host is up"
                            " and credentials are correct", 0);
        }
#endif
        LOG(LOG_LEVEL_ERROR, "NeutrinoRDP proxy connection: status [Failed],"
            " RDP client [%s], RDP server [%s:%d], RDP server username [%s],"
            " xrdp pamusername [%s], xrdp process id [%d]",
            mod->client_info.client_description,
            mod->inst->settings->hostname,
            mod->inst->settings->port,
            mod->inst->settings->username,
            mod->pamusername,
            g_getpid());
        return 1;
    }
    else
    {
        LOG(LOG_LEVEL_INFO, "NeutrinoRDP proxy connection: status [Success],"
            " RDP client [%s], RDP server [%s:%d], RDP server username [%s],"
            " xrdp pamusername [%s], xrdp process id [%d]",
            mod->client_info.client_description,
            mod->inst->settings->hostname,
            mod->inst->settings->port,
            mod->inst->settings->username,
            mod->pamusername,
            g_getpid());
    }

    return 0;
}

/******************************************************************************/
/* return error */
static int
lxrdp_event(struct mod *mod, int msg, long param1, long param2,
            long param3, long param4)
{
    int x;
    int y;
    int cx;
    int cy;
    int flags;
    int size;
    int total_size;
    int chanid;
    int lchid;
    char *data;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "lxrdp_event: msg %d", msg);

    switch (msg)
    {
        case WM_KEYDOWN:

            /* Before we handle the first character we synchronize
               capslock and numlock. */
            /* We collect the state during the first synchronize
               ( see msg 17) */
            if (!mod->bool_keyBoardSynced)
            {
                LOG_DEVEL(LOG_LEVEL_DEBUG, "Additional Sync event handled : %d", mod->keyBoardLockInfo);
                mod->inst->input->SynchronizeEvent(mod->inst->input, mod->keyBoardLockInfo);
                mod->bool_keyBoardSynced = 1;
            }

            mod->inst->input->KeyboardEvent(mod->inst->input, param4, param3);
            break;

        case WM_KEYUP:
            mod->inst->input->KeyboardEvent(mod->inst->input, param4, param3);
            break;

        case WM_KEYBRD_SYNC:
            LOG_DEVEL(LOG_LEVEL_DEBUG, "Synchronized event handled : %ld", param1);
            /* In some situations the Synchronize event come to early.
               Therefore we store this information and use it when we
               receive the first keyboard event
               Without this fix numlock and capslock can come
               out of sync. */
            mod->inst->input->SynchronizeEvent(mod->inst->input, param1);

            if (!mod->bool_keyBoardSynced)
            {
                mod->keyBoardLockInfo = param1;
            }

            break;

        case WM_MOUSEMOVE: /* mouse move */
            LOG_DEVEL(LOG_LEVEL_DEBUG, "mouse move %ld %ld", param1, param2);
            x = param1;
            y = param2;
            flags = PTR_FLAGS_MOVE;
            mod->inst->input->MouseEvent(mod->inst->input, flags, x, y);
            break;

        case WM_LBUTTONUP:
            LOG_DEVEL(LOG_LEVEL_DEBUG, "left button up %ld %ld", param1, param2);
            x = param1;
            y = param2;
            flags = PTR_FLAGS_BUTTON1;
            mod->inst->input->MouseEvent(mod->inst->input, flags, x, y);
            break;

        case WM_LBUTTONDOWN:
            LOG_DEVEL(LOG_LEVEL_DEBUG, "left button down %ld %ld", param1, param2);
            x = param1;
            y = param2;
            flags = PTR_FLAGS_BUTTON1 | PTR_FLAGS_DOWN;
            mod->inst->input->MouseEvent(mod->inst->input, flags, x, y);
            break;

        case WM_RBUTTONUP:
            LOG_DEVEL(LOG_LEVEL_DEBUG, "right button up %ld %ld", param1, param2);
            x = param1;
            y = param2;
            flags = PTR_FLAGS_BUTTON2;
            mod->inst->input->MouseEvent(mod->inst->input, flags, x, y);
            break;

        case WM_RBUTTONDOWN:
            LOG_DEVEL(LOG_LEVEL_DEBUG, "right button down %ld %ld", param1, param2);
            x = param1;
            y = param2;
            flags = PTR_FLAGS_BUTTON2 | PTR_FLAGS_DOWN;
            mod->inst->input->MouseEvent(mod->inst->input, flags, x, y);
            break;

        case WM_BUTTON3UP: /* middle button up */
            LOG_DEVEL(LOG_LEVEL_DEBUG, "middle button up %ld %ld", param1, param2);
            x = param1;
            y = param2;
            flags = PTR_FLAGS_BUTTON3;
            mod->inst->input->MouseEvent(mod->inst->input, flags, x, y);
            break;

        case WM_BUTTON3DOWN: /* middle button down */
            LOG_DEVEL(LOG_LEVEL_DEBUG, "middle button down %ld %ld", param1, param2);
            x = param1;
            y = param2;
            flags = PTR_FLAGS_BUTTON3 | PTR_FLAGS_DOWN;
            mod->inst->input->MouseEvent(mod->inst->input, flags, x, y);
            break;

        case WM_BUTTON4UP: /* wheel up */
            flags = PTR_FLAGS_WHEEL | 0x0078;
            mod->inst->input->MouseEvent(mod->inst->input, flags, 0, 0);
            break;

        case WM_BUTTON4DOWN:
            break;

        case WM_BUTTON5UP: /* wheel down */
            flags = PTR_FLAGS_WHEEL | PTR_FLAGS_WHEEL_NEGATIVE | 0x0088;
            mod->inst->input->MouseEvent(mod->inst->input, flags, 0, 0);
            break;

        case WM_BUTTON5DOWN:
            break;

        case WM_BUTTON8UP:
            LOG_DEVEL(LOG_LEVEL_DEBUG, "extended mouse button8 up %ld %ld", param1, param2);
            x = param1;
            y = param2;
            flags = PTR_XFLAGS_BUTTON1;
            mod->inst->input->ExtendedMouseEvent(mod->inst->input, flags, x, y);
            break;

        case WM_BUTTON8DOWN:
            LOG_DEVEL(LOG_LEVEL_DEBUG, "extended mouse button8 down %ld %ld", param1, param2);
            x = param1;
            y = param2;
            flags = PTR_XFLAGS_BUTTON1 | PTR_XFLAGS_DOWN;
            mod->inst->input->ExtendedMouseEvent(mod->inst->input, flags, x, y);
            break;

        case WM_BUTTON9UP:
            LOG_DEVEL(LOG_LEVEL_DEBUG, "extended mouse button9 up %ld %ld", param1, param2);
            x = param1;
            y = param2;
            flags = PTR_XFLAGS_BUTTON2;
            mod->inst->input->ExtendedMouseEvent(mod->inst->input, flags, x, y);
            break;

        case WM_BUTTON9DOWN:
            LOG_DEVEL(LOG_LEVEL_DEBUG, "extended mouse button9 down %ld %ld", param1, param2);
            x = param1;
            y = param2;
            flags = PTR_XFLAGS_BUTTON2 | PTR_XFLAGS_DOWN;
            mod->inst->input->ExtendedMouseEvent(mod->inst->input, flags, x, y);
            break;

        case WM_INVALIDATE:
            LOG_DEVEL(LOG_LEVEL_DEBUG, "Invalidate request sent from client");
            x = (param1 >> 16) & 0xffff;
            y = (param1 >> 0) & 0xffff;
            cx = (param2 >> 16) & 0xffff;
            cy = (param2 >> 0) & 0xffff;
            mod->inst->SendInvalidate(mod->inst, -1, x, y, cx, cy);
            break;

        case WM_CHANNEL_DATA:
            chanid = LOWORD(param1);
            flags = HIWORD(param1);
            size = (int)param2;
            data = (char *)param3;
            total_size = (int)param4;

            LOG_DEVEL(LOG_LEVEL_DEBUG, "lxrdp_event: client to server ,chanid= %d  flags= %d", chanid, flags);

            if ((chanid < 0) || (chanid >= mod->inst->settings->num_channels))
            {
                LOG(LOG_LEVEL_WARNING, "lxrdp_event: error chanid %d", chanid);
                break;
            }

            lchid = mod->inst->settings->channels[chanid].channel_id;

            switch (flags & 3)
            {
                case 3:
                    mod->inst->SendChannelData(mod->inst, lchid, (tui8 *)data, total_size);
                    break;

                case 2:
                    /* end */
                    g_memcpy(mod->chan_buf + mod->chan_buf_valid, data, size);
                    mod->chan_buf_valid += size;
                    mod->inst->SendChannelData(mod->inst, lchid, (tui8 *)(mod->chan_buf),
                                               total_size);
                    g_free(mod->chan_buf);
                    mod->chan_buf = 0;
                    mod->chan_buf_bytes = 0;
                    mod->chan_buf_valid = 0;
                    break;

                case 1:
                    /* start */
                    g_free(mod->chan_buf);
                    mod->chan_buf = (char *)g_malloc(total_size, 0);
                    mod->chan_buf_bytes = total_size;
                    mod->chan_buf_valid = 0;
                    g_memcpy(mod->chan_buf + mod->chan_buf_valid, data, size);
                    mod->chan_buf_valid += size;
                    break;

                default:
                    /* middle */
                    g_memcpy(mod->chan_buf + mod->chan_buf_valid, data, size);
                    mod->chan_buf_valid += size;
                    break;
            }

            break;

        default:
            LOG(LOG_LEVEL_WARNING, "Unhandled message type in eventhandler %d", msg);
            break;
    }

    return 0;
}

/******************************************************************************/
/* return error */
static int
lxrdp_signal(struct mod *mod)
{
    LOG_DEVEL(LOG_LEVEL_DEBUG, "lxrdp_signal:");
    return 0;
}

/******************************************************************************/
/* return error */
static int
lxrdp_end(struct mod *mod)
{
    int i;
    int j;

    for (j = 0; j < 4; j++)
    {
        for (i = 0; i < 4096; i++)
        {
            g_free(mod->bitmap_cache[j][i].data);
        }
    }

    for (i = 0; i < 64; i++)
    {
        if (mod->brush_cache[i].data != mod->brush_cache[i].b8x8)
        {
            g_free(mod->brush_cache[i].data);
        }
    }

    LOG_DEVEL(LOG_LEVEL_DEBUG, "lxrdp_end:");
    LOG(LOG_LEVEL_INFO, "NeutrinoRDP proxy connection: status [Disconnect],"
        " RDP client [%s], RDP server [%s:%d], RDP server username [%s],"
        " xrdp pamusername [%s], xrdp process id [%d]",
        mod->client_info.client_description,
        mod->inst->settings->hostname,
        mod->inst->settings->port,
        mod->inst->settings->username,
        mod->pamusername,
        g_getpid());
    return 0;
}

/******************************************************************************/
/* return error */
static int
lxrdp_set_param(struct mod *mod, const char *name, const char *value)
{
    rdpSettings *settings;

    if (g_strcmp(name, "password") == 0 || g_strcmp(name, "pampassword") == 0)
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "lxrdp_set_param: name [%s] value [******]", name);
    }
    else
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "lxrdp_set_param: name [%s] value [%s]", name, value);
    }

    settings = mod->inst->settings;

    if (g_strcmp(name, "hostname") == 0)
    {
    }
    else if (g_strcmp(name, "ip") == 0)
    {
        settings->hostname = g_strdup(value);
    }
    else if (g_strcmp(name, "port") == 0)
    {
        settings->port = g_atoi(value);
    }
    else if (g_strcmp(name, "keylayout") == 0)
    {
        LOG(LOG_LEVEL_DEBUG, "%s:[0x%08X]", name, g_atoi(value));
    }
    else if (g_strcmp(name, "name") == 0)
    {
    }
    else if (g_strcmp(name, "lib") == 0)
    {
    }
    else if (g_strcmp(name, "username") == 0)
    {
        g_strncpy(mod->username, value, 255);
    }
    else if (g_strcmp(name, "domain") == 0)
    {
        g_strncpy(mod->domain, value, 255);
    }
    else if (g_strcmp(name, "password") == 0)
    {
        g_strncpy(mod->password, value, 255);
    }
    else if (g_strcmp(name, "client_info") == 0)
    {
        g_memcpy(&(mod->client_info), value, sizeof(mod->client_info));
        /* This is a Struct and cannot be printed in next else*/
        LOG_DEVEL(LOG_LEVEL_DEBUG, "Client_info struct ignored");
    }
    else if (g_strcmp(name, "program") == 0)
    {
        settings->shell = g_strdup(value);
    }
    else if (g_strcmp(name, "nla") == 0)
    {
        settings->nla_security = g_text2bool(value);
    }
    else if (g_strcmp(name, "enable_dynamic_resizing") == 0)
    {
        settings->desktop_resize = g_text2bool(value);
    }
    else if (g_strcmp(name, "pamusername") == 0)
    {
        g_strncpy(mod->pamusername, value, 255);
    }
    else if (g_strcmp(name, "pampassword") == 0 ||
             g_strcmp(name, "pamsessionmng") == 0)
    {
        /* Valid (but unused) parameters not logged */
    }
    else if (g_strcmp(name, "channel.rdpdr") == 0 ||
             g_strcmp(name, "channel.rdpsnd") == 0 ||
             g_strcmp(name, "channel.cliprdr") == 0 ||
             g_strcmp(name, "channel.drdynvc") == 0)
    {
        /* Valid (but unused) parameters not logged */
    }
    else if (g_strcmp(name, "perf.allow_client_experiencesettings") == 0)
    {
        mod->allow_client_experiencesettings = g_text2bool(value);
    }
    else if (g_strcmp(name, "perf.wallpaper") == 0)
    {
        mod->perf_settings_override_mask |= PERF_DISABLE_WALLPAPER;
        if (!g_text2bool(value))
        {
            mod->perf_settings_values_mask |= PERF_DISABLE_WALLPAPER;
        }
    }
    else if (g_strcmp(name, "perf.font_smoothing") == 0)
    {
        mod->perf_settings_override_mask |= PERF_ENABLE_FONT_SMOOTHING;
        if (g_text2bool(value))
        {
            mod->perf_settings_values_mask |= PERF_ENABLE_FONT_SMOOTHING;
        }
    }
    else if (g_strcmp(name, "perf.desktop_composition") == 0)
    {
        mod->perf_settings_override_mask |= PERF_ENABLE_DESKTOP_COMPOSITION;
        if (g_text2bool(value))
        {
            mod->perf_settings_values_mask |= PERF_ENABLE_DESKTOP_COMPOSITION;
        }
    }
    else if (g_strcmp(name, "perf.full_window_drag") == 0)
    {
        mod->perf_settings_override_mask |= PERF_DISABLE_FULLWINDOWDRAG;
        if (!g_text2bool(value))
        {
            mod->perf_settings_values_mask |= PERF_DISABLE_FULLWINDOWDRAG;
        }
    }
    else if (g_strcmp(name, "perf.menu_anims") == 0)
    {
        mod->perf_settings_override_mask |= PERF_DISABLE_MENUANIMATIONS;
        if (!g_text2bool(value))
        {
            mod->perf_settings_values_mask |= PERF_DISABLE_MENUANIMATIONS;
        }
    }
    else if (g_strcmp(name, "perf.themes") == 0)
    {
        mod->perf_settings_override_mask |= PERF_DISABLE_THEMING;
        if (!g_text2bool(value))
        {
            mod->perf_settings_values_mask |= PERF_DISABLE_THEMING;
        }
    }
    else if (g_strcmp(name, "perf.cursor_blink") == 0)
    {
        mod->perf_settings_override_mask |= PERF_DISABLE_CURSORSETTINGS;
        if (!g_text2bool(value))
        {
            mod->perf_settings_values_mask |= PERF_DISABLE_CURSORSETTINGS;
        }
    }
    else if (g_strcmp(name, "perf.cursor_shadow") == 0)
    {
        mod->perf_settings_override_mask |= PERF_DISABLE_CURSOR_SHADOW;
        if (!g_text2bool(value))
        {
            mod->perf_settings_values_mask |= PERF_DISABLE_CURSOR_SHADOW;
        }
    }
    else if (g_strcmp(name, "neutrinordp.allow_client_keyboardLayout") == 0)
    {
        mod->allow_client_kbd_settings = g_text2bool(value);
    }
    else if (g_strcmp(name, "neutrinordp.override_keyboardLayout_mask") == 0)
    {
        /* Keyboard values are stored for later processing */
        mod->kbd_overrides.layout_mask = g_atoix(value);
    }
    else if (g_strcmp(name, "neutrinordp.override_kbd_type") == 0)
    {
        mod->kbd_overrides.type = g_atoix(value);
    }
    else if (g_strcmp(name, "neutrinordp.override_kbd_subtype") == 0)
    {
        mod->kbd_overrides.subtype = g_atoix(value);
    }
    else if (g_strcmp(name, "neutrinordp.override_kbd_fn_keys") == 0)
    {
        mod->kbd_overrides.fn_keys = g_atoix(value);
    }
    else if (g_strcmp(name, "neutrinordp.override_kbd_layout") == 0)
    {
        mod->kbd_overrides.layout = g_atoix(value);
    }
    else
    {
        LOG(LOG_LEVEL_WARNING, "lxrdp_set_param: unknown name [%s] value [%s]", name, value);
    }

    return 0;
}

/******************************************************************************/
static int
lxrdp_session_change(struct mod *mod, int a, int b)
{
    LOG_DEVEL(LOG_LEVEL_DEBUG, "lxrdp_session_change: - no code here");
    return 0;
}

/******************************************************************************/
static int
lxrdp_get_wait_objs(struct mod *mod, tbus *read_objs, int *rcount,
                    tbus *write_objs, int *wcount, int *timeout)
{
    void **rfds;
    void **wfds;
    boolean ok;

    LOG_DEVEL(LOG_LEVEL_TRACE, "lxrdp_get_wait_objs:");
    /*
     * Don't check this module for activity if our queued output data
     * has already reached the limit
     */
    if (get_queued_module_output_data(mod) > MAX_QUEUED_MODULE_OUTPUT_DATA)
    {
        *rcount = 0;
        *wcount = 0;
    }
    else
    {
        rfds = (void **)read_objs;
        wfds = (void **)write_objs;
        ok = freerdp_get_fds(mod->inst, rfds, rcount, wfds, wcount);

        if (!ok)
        {
            LOG(LOG_LEVEL_ERROR, "lxrdp_get_wait_objs: freerdp_get_fds failed");
            return 1;
        }
    }

    return 0;
}

/******************************************************************************/
static int
lxrdp_check_wait_objs(struct mod *mod)
{
    boolean ok;

    LOG_DEVEL(LOG_LEVEL_TRACE, "lxrdp_check_wait_objs:");
    /*
     * Only process the freerdp file descriptors if our queued output data
     * has not reached the limit
     */
    if (get_queued_module_output_data(mod) <= MAX_QUEUED_MODULE_OUTPUT_DATA)
    {
        /*
         * Before checking the file descriptors, set the source info
         * current source, so any data queued on output trans objects
         * gets attributed to this module
         */
        if (mod->si)
        {
            mod->si->cur_source = XRDP_SOURCE_MOD;
        }
        ok = freerdp_check_fds(mod->inst);
        if (mod->si)
        {
            mod->si->cur_source = XRDP_SOURCE_NONE;
        }

        if (!ok)
        {
            LOG(LOG_LEVEL_ERROR, "lxrdp_check_wait_objs: freerdp_check_fds failed");
            return 1;
        }
    }

    return 0;
}

/******************************************************************************/
static int
lxrdp_frame_ack(struct mod *mod, int flags, int frame_id)
{
    return 0;
}

/******************************************************************************/
static int
lxrdp_suppress_output(struct mod *mod, int suppress,
                      int left, int top, int right, int bottom)
{
#if defined(NEUTRINORDP_HAS_SUPPRESS_OUTPUT)
    mod->inst->SendSuppressOutput(mod->inst, !suppress, left, top, right, bottom);
#endif
    return 0;
}

/******************************************************************************/
static int
lxrdp_server_version_message(struct mod *mod)
{
    return 0;
}

/******************************************************************************/
static int
lxrdp_server_monitor_resize(struct mod *mod, int width, int height,
                            int num_monitors,
                            const struct monitor_info *monitors,
                            int *in_progress)
{
    *in_progress = 0;
    return 0;
}

/******************************************************************************/
static int
lxrdp_server_monitor_full_invalidate(struct mod *mod, int width, int height)
{
    return 0;
}

/******************************************************************************/
static void
lfreerdp_begin_paint(rdpContext *context)
{
    struct mod *mod;

    LOG_DEVEL(LOG_LEVEL_TRACE, "lfreerdp_begin_paint:");
    mod = ((struct mod_context *)context)->modi;
    mod->server_begin_update(mod);
}

/******************************************************************************/
static void
lfreerdp_end_paint(rdpContext *context)
{
    struct mod *mod;

    LOG_DEVEL(LOG_LEVEL_TRACE, "lfreerdp_end_paint:");
    mod = ((struct mod_context *)context)->modi;
    mod->server_end_update(mod);
}

/******************************************************************************/
static void
lfreerdp_set_bounds(rdpContext *context, rdpBounds *bounds)
{
    struct mod *mod;
    int x;
    int y;
    int cx;
    int cy;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "lfreerdp_set_bounds: %p", bounds);
    mod = ((struct mod_context *)context)->modi;

    if (bounds != 0)
    {
        x = bounds->left;
        y = bounds->top;
        cx = (bounds->right - bounds->left) + 1;
        cy = (bounds->bottom - bounds->top) + 1;
        mod->server_set_clip(mod, x, y, cx, cy);
    }
    else
    {
        mod->server_reset_clip(mod);
    }
}

/******************************************************************************/
static void
lfreerdp_bitmap_update(rdpContext *context, BITMAP_UPDATE *bitmap)
{
    struct mod *mod;
    int index;
    int cx;
    int cy;
    int server_bpp;
    int server_Bpp;
    int client_bpp;
    int j;
    int line_bytes;
    BITMAP_DATA *bd;
    char *dst_data;
    char *dst_data1;
    char *src;
    char *dst;

    mod = ((struct mod_context *)context)->modi;
    LOG_DEVEL(LOG_LEVEL_DEBUG, "lfreerdp_bitmap_update: %d %d", bitmap->number, bitmap->count);

    server_bpp = mod->inst->settings->color_depth;
    server_Bpp = (server_bpp + 7) / 8;
    client_bpp = mod->bpp;

    for (index = 0; index < bitmap->number; index++)
    {
        bd = &bitmap->rectangles[index];
        cx = (bd->destRight - bd->destLeft) + 1;
        cy = (bd->destBottom - bd->destTop) + 1;
        line_bytes = server_Bpp * bd->width;
        dst_data = (char *)g_malloc(bd->height * line_bytes + 16, 0);

        if (bd->compressed)
        {
            LOG_DEVEL(LOG_LEVEL_DEBUG, "decompress size : %d", bd->bitmapLength);

            if (!bitmap_decompress(bd->bitmapDataStream, (tui8 *)dst_data, bd->width,
                                   bd->height, bd->bitmapLength, server_bpp, server_bpp))
            {
                LOG(LOG_LEVEL_WARNING, "Failure to decompress the bitmap");
            }
        }
        else
        {
            /* bitmap is upside down */
            LOG_DEVEL(LOG_LEVEL_DEBUG, "bitmap upside down");
            src = (char *)(bd->bitmapDataStream);
            dst = dst_data + bd->height * line_bytes;

            for (j = 0; j < bd->height; j++)
            {
                dst -= line_bytes;
                g_memcpy(dst, src, line_bytes);
                src += line_bytes;
            }
        }

        dst_data1 = convert_bitmap(server_bpp, client_bpp, dst_data,
                                   bd->width, bd->height, mod->colormap);
        mod->server_paint_rect(mod, bd->destLeft, bd->destTop, cx, cy,
                               dst_data1, bd->width, bd->height, 0, 0);

        if (dst_data1 != dst_data)
        {
            g_free(dst_data1);
        }

        g_free(dst_data);
    }
}

/******************************************************************************/
static void
lfreerdp_dst_blt(rdpContext *context, DSTBLT_ORDER *dstblt)
{
    struct mod *mod;

    mod = ((struct mod_context *)context)->modi;
    LOG_DEVEL(LOG_LEVEL_TRACE, "lfreerdp_dst_blt:");
    mod->server_set_opcode(mod, dstblt->bRop);
    mod->server_fill_rect(mod, dstblt->nLeftRect, dstblt->nTopRect,
                          dstblt->nWidth, dstblt->nHeight);
    mod->server_set_opcode(mod, 0xcc);
}

/******************************************************************************/
static void
lfreerdp_pat_blt(rdpContext *context, PATBLT_ORDER *patblt)
{
    struct mod *mod;
    int idx;
    int fgcolor;
    int bgcolor;
    int server_bpp;
    int client_bpp;
    struct brush_item *bi;

    mod = ((struct mod_context *)context)->modi;
    LOG_DEVEL(LOG_LEVEL_TRACE, "lfreerdp_pat_blt:");

    server_bpp = mod->inst->settings->color_depth;
    client_bpp = mod->bpp;
    LOG_DEVEL(LOG_LEVEL_DEBUG, "lfreerdp_pat_blt: bpp %d %d", server_bpp, client_bpp);

    fgcolor = convert_color(server_bpp, client_bpp,
                            patblt->foreColor, mod->colormap);
    bgcolor = convert_color(server_bpp, client_bpp,
                            patblt->backColor, mod->colormap);

    LOG_DEVEL(LOG_LEVEL_DEBUG, "lfreerdp_pat_blt: nLeftRect %d nTopRect %d "
              "nWidth %d nHeight %d fgcolor 0x%8.8x bgcolor 0x%8.8x",
              patblt->nLeftRect, patblt->nTopRect,
              patblt->nWidth, patblt->nHeight, fgcolor, bgcolor);

    if (fgcolor == bgcolor)
    {
        LOG(LOG_LEVEL_WARNING, "Warning same color on both bg and fg");
    }

    mod->server_set_mixmode(mod, 1);
    mod->server_set_opcode(mod, patblt->bRop);
    mod->server_set_fgcolor(mod, fgcolor);
    mod->server_set_bgcolor(mod, bgcolor);

    if (patblt->brush.style & 0x80)
    {
        idx = patblt->brush.hatch;

        if ((idx < 0) || (idx >= 64))
        {
            LOG(LOG_LEVEL_ERROR, "lfreerdp_pat_blt: error patblt->brush.hatch, "
                "Expected min 0, max 63. Actual %d", idx);
            return;
        }

        bi = mod->brush_cache + idx;
        mod->server_set_brush(mod, patblt->brush.x, patblt->brush.y,
                              3, bi->b8x8);
    }
    else
    {
        mod->server_set_brush(mod, patblt->brush.x, patblt->brush.y,
                              patblt->brush.style,
                              (char *)(patblt->brush.p8x8));
    }

    mod->server_fill_rect(mod, patblt->nLeftRect, patblt->nTopRect,
                          patblt->nWidth, patblt->nHeight);
    mod->server_set_opcode(mod, 0xcc);
    mod->server_set_mixmode(mod, 0);

}

/******************************************************************************/
static void
lfreerdp_scr_blt(rdpContext *context, SCRBLT_ORDER *scrblt)
{
    struct mod *mod;

    mod = ((struct mod_context *)context)->modi;
    LOG_DEVEL(LOG_LEVEL_TRACE, "lfreerdp_scr_blt:");
    mod->server_set_opcode(mod, scrblt->bRop);
    mod->server_screen_blt(mod, scrblt->nLeftRect, scrblt->nTopRect,
                           scrblt->nWidth, scrblt->nHeight,
                           scrblt->nXSrc, scrblt->nYSrc);
    mod->server_set_opcode(mod, 0xcc);
}

/******************************************************************************/
static void
lfreerdp_opaque_rect(rdpContext *context, OPAQUE_RECT_ORDER *opaque_rect)
{
    struct mod *mod;
    int server_bpp;
    int client_bpp;
    int fgcolor;

    mod = ((struct mod_context *)context)->modi;
    LOG_DEVEL(LOG_LEVEL_TRACE, "lfreerdp_opaque_rect:");
    server_bpp = mod->inst->settings->color_depth;
    client_bpp = mod->bpp;
    fgcolor = convert_color(server_bpp, client_bpp,
                            opaque_rect->color, mod->colormap);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "lfreerdp_opaque_rect: nLeftRect %d nTopRect %d "
              "nWidth %d nHeight %d fgcolor 0x%8.8x",
              opaque_rect->nLeftRect, opaque_rect->nTopRect,
              opaque_rect->nWidth, opaque_rect->nHeight, fgcolor);
    mod->server_set_fgcolor(mod, fgcolor);
    mod->server_fill_rect(mod, opaque_rect->nLeftRect, opaque_rect->nTopRect,
                          opaque_rect->nWidth, opaque_rect->nHeight);
}

/******************************************************************************/
static void
lfreerdp_mem_blt(rdpContext *context, MEMBLT_ORDER *memblt)
{
    int id;
    int idx;
    struct mod *mod;
    struct bitmap_item *bi;

    mod = ((struct mod_context *)context)->modi;
    LOG_DEVEL(LOG_LEVEL_DEBUG, "lfreerdp_mem_blt: cacheId %d cacheIndex %d",
              memblt->cacheId, memblt->cacheIndex);

    id = memblt->cacheId;
    idx = memblt->cacheIndex;

    if (idx == 32767) /* BITMAPCACHE_WAITING_LIST_INDEX */
    {
        idx = 4096 - 1;
    }

    if ((id < 0) || (id >= 4))
    {
        LOG(LOG_LEVEL_ERROR, "lfreerdp_mem_blt: bad id [%d]", id);
        return;
    }

    if ((idx < 0) || (idx >= 4096))
    {
        LOG(LOG_LEVEL_ERROR, "lfreerdp_mem_blt: bad idx [%d]", idx);
        return;
    }

    bi = &(mod->bitmap_cache[id][idx]);

    mod->server_set_opcode(mod, memblt->bRop);
    mod->server_paint_rect(mod, memblt->nLeftRect, memblt->nTopRect,
                           memblt->nWidth, memblt->nHeight,
                           bi->data, bi->width, bi->height,
                           memblt->nXSrc, memblt->nYSrc);
    mod->server_set_opcode(mod, 0xcc);

}

/******************************************************************************/
static void
lfreerdp_glyph_index(rdpContext *context, GLYPH_INDEX_ORDER *glyph_index)
{
    struct mod *mod;
    int server_bpp;
    int client_bpp;
    int fgcolor;
    int bgcolor;
    int opLeft;
    int opTop;
    int opRight;
    int opBottom;

    mod = ((struct mod_context *)context)->modi;
    LOG_DEVEL(LOG_LEVEL_TRACE, "lfreerdp_glyph_index:");
    server_bpp = mod->inst->settings->color_depth;
    client_bpp = mod->bpp;
    fgcolor = convert_color(server_bpp, client_bpp,
                            glyph_index->foreColor, mod->colormap);
    bgcolor = convert_color(server_bpp, client_bpp,
                            glyph_index->backColor, mod->colormap);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "lfreerdp_glyph_index: "
              "bkLeft %d bkTop %d width %d height %d "
              "opLeft %d opTop %d width %d height %d "
              "cbData %d fgcolor 0x%8.8x bgcolor 0x%8.8x fOpRedundant %d",
              glyph_index->bkLeft, glyph_index->bkTop,
              glyph_index->bkRight - glyph_index->bkLeft,
              glyph_index->bkBottom - glyph_index->bkTop,
              glyph_index->opLeft, glyph_index->opTop,
              glyph_index->opRight - glyph_index->opLeft,
              glyph_index->opBottom - glyph_index->opTop,
              glyph_index->cbData, fgcolor, bgcolor, glyph_index->fOpRedundant);
    mod->server_set_bgcolor(mod, fgcolor);
    mod->server_set_fgcolor(mod, bgcolor);
    opLeft = glyph_index->opLeft;
    opTop = glyph_index->opTop;
    opRight = glyph_index->opRight;
    opBottom = glyph_index->opBottom;
#if 1

    /* workarounds for freerdp not using fOpRedundant in
       glyph.c::update_gdi_glyph_index */
    if (glyph_index->fOpRedundant)
    {
        opLeft = glyph_index->bkLeft;
        opTop = glyph_index->bkTop;
        opRight = glyph_index->bkRight;
        opBottom = glyph_index->bkBottom;
    }

#endif
    mod->server_draw_text(mod, glyph_index->cacheId, glyph_index->flAccel,
                          glyph_index->fOpRedundant,
                          glyph_index->bkLeft, glyph_index->bkTop,
                          glyph_index->bkRight, glyph_index->bkBottom,
                          opLeft, opTop, opRight, opBottom,
                          glyph_index->x, glyph_index->y,
                          (char *)(glyph_index->data), glyph_index->cbData);
}

/******************************************************************************/
static void
lfreerdp_line_to(rdpContext *context, LINE_TO_ORDER *line_to)
{
    struct mod *mod;
    int server_bpp;
    int client_bpp;
    int fgcolor;
    int bgcolor;

    mod = ((struct mod_context *)context)->modi;
    LOG_DEVEL(LOG_LEVEL_TRACE, "lfreerdp_line_to:");
    mod->server_set_opcode(mod, line_to->bRop2);
    server_bpp = mod->inst->settings->color_depth;
    client_bpp = mod->bpp;
    fgcolor = convert_color(server_bpp, client_bpp,
                            line_to->penColor, mod->colormap);
    bgcolor = convert_color(server_bpp, client_bpp,
                            line_to->backColor, mod->colormap);
    mod->server_set_fgcolor(mod, fgcolor);
    mod->server_set_bgcolor(mod, bgcolor);
    mod->server_set_pen(mod, line_to->penStyle, line_to->penWidth);
    mod->server_draw_line(mod, line_to->nXStart, line_to->nYStart,
                          line_to->nXEnd, line_to->nYEnd);
    mod->server_set_opcode(mod, 0xcc);
}

/******************************************************************************/
static void
lfreerdp_cache_bitmap(rdpContext *context, CACHE_BITMAP_ORDER *cache_bitmap_order)
{
    LOG_DEVEL(LOG_LEVEL_INFO, "lfreerdp_cache_bitmap: - no code here");
}

/******************************************************************************/
/* Turn the bitmap upside down*/
static void
lfreerdp_upsidedown(uint8 *destination, CACHE_BITMAP_V2_ORDER *cache_bitmap_v2_order, int server_Bpp)
{
    tui8 *src;
    tui8 *dst;
    int line_bytes;
    int j;

    if (destination == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "lfreerdp_upsidedown: destination pointer is NULL !!!");
        return;
    }

    line_bytes = server_Bpp * cache_bitmap_v2_order->bitmapWidth;
    src = cache_bitmap_v2_order->bitmapDataStream;
    dst = destination + ((cache_bitmap_v2_order->bitmapHeight) * line_bytes);

    for (j = 0; j < cache_bitmap_v2_order->bitmapHeight; j++)
    {
        dst -= line_bytes;
        g_memcpy(dst, src, line_bytes);
        src += line_bytes;
    }
}

/******************************************************************************/
static void
lfreerdp_cache_bitmapV2(rdpContext *context,
                        CACHE_BITMAP_V2_ORDER *cache_bitmap_v2_order)
{
    char *dst_data;
    char *dst_data1;
    int bytes;
    int width;
    int height;
    int id;
    int idx;
    int flags;
    int server_bpp;
    int server_Bpp;
    int client_bpp;
    struct mod *mod;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "lfreerdp_cache_bitmapV2: %d %d 0x%8.8x compressed %d",
              cache_bitmap_v2_order->cacheId,
              cache_bitmap_v2_order->cacheIndex,
              cache_bitmap_v2_order->flags,
              cache_bitmap_v2_order->compressed);

    mod = ((struct mod_context *)context)->modi;
    id = cache_bitmap_v2_order->cacheId;
    idx = cache_bitmap_v2_order->cacheIndex;
    flags = cache_bitmap_v2_order->flags;

    if (flags & 0x10) /* CBR2_DO_NOT_CACHE */
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "lfreerdp_cache_bitmapV2: CBR2_DO_NOT_CACHE");
        idx = 4096 - 1;
    }

    if ((id < 0) || (id >= 4))
    {
        LOG(LOG_LEVEL_ERROR, "lfreerdp_cache_bitmapV2: bad id [%d]", id);
        return;
    }

    if ((idx < 0) || (idx >= 4096))
    {
        LOG(LOG_LEVEL_ERROR, "lfreerdp_cache_bitmapV2: bad idx [%d]", idx);
        return;
    }

    server_bpp = mod->inst->settings->color_depth;
    server_Bpp = (server_bpp + 7) / 8;
    client_bpp = mod->bpp;

    width = cache_bitmap_v2_order->bitmapWidth;
    height = cache_bitmap_v2_order->bitmapHeight;
    bytes = width * height * server_Bpp + 16;
    dst_data = (char *)g_malloc(bytes, 0);

    if (cache_bitmap_v2_order->compressed)
    {
        bitmap_decompress(cache_bitmap_v2_order->bitmapDataStream,
                          (tui8 *)dst_data, width, height,
                          cache_bitmap_v2_order->bitmapLength,
                          server_bpp, server_bpp);
    }
    else
    {
        /* Uncompressed bitmaps are upside down */
        lfreerdp_upsidedown((tui8 *)dst_data, cache_bitmap_v2_order, server_Bpp);
        LOG_DEVEL(LOG_LEVEL_DEBUG, "lfreerdp_cache_bitmapV2:  upside down progressed");
    }

    dst_data1 = convert_bitmap(server_bpp, client_bpp, dst_data,
                               width, height, mod->colormap);
    g_free(mod->bitmap_cache[id][idx].data);
    mod->bitmap_cache[id][idx].width = width;
    mod->bitmap_cache[id][idx].height = height;
    mod->bitmap_cache[id][idx].data = dst_data1;

    if (dst_data != dst_data1)
    {
        g_free(dst_data);
    }
}

/******************************************************************************/
static void
lfreerdp_cache_glyph(rdpContext *context, CACHE_GLYPH_ORDER *cache_glyph_order)
{
    int index;
    GLYPH_DATA *gd;
    struct mod *mod;

    mod = ((struct mod_context *)context)->modi;
    LOG_DEVEL(LOG_LEVEL_DEBUG, "lfreerdp_cache_glyph: %d", cache_glyph_order->cGlyphs);

    for (index = 0; index < cache_glyph_order->cGlyphs; index++)
    {
        gd = cache_glyph_order->glyphData[index];
        LOG_DEVEL(LOG_LEVEL_DEBUG, "  %d %d %d %d %d", gd->cacheIndex, gd->x, gd->y,
                  gd->cx, gd->cy);
        mod->server_add_char(mod, cache_glyph_order->cacheId, gd->cacheIndex,
                             gd->x, gd->y, gd->cx, gd->cy, (char *)(gd->aj));
        free(gd->aj);
        gd->aj = 0;
        free(gd);
        cache_glyph_order->glyphData[index] = 0;
    }

    free(cache_glyph_order->unicodeCharacters);
    cache_glyph_order->unicodeCharacters = 0;
}

/******************************************************************************/
static void
lfreerdp_cache_brush(rdpContext *context, CACHE_BRUSH_ORDER *cache_brush_order)
{
    int idx;
    int bytes;
    int bpp;
    int cx;
    int cy;
    struct mod *mod;

    mod = ((struct mod_context *)context)->modi;
    bpp = cache_brush_order->bpp;
    cx = cache_brush_order->cx;
    cy = cache_brush_order->cy;
    idx = cache_brush_order->index;
    bytes = cache_brush_order->length;
    LOG_DEVEL(LOG_LEVEL_DEBUG, "lfreerdp_cache_brush: bpp %d cx %d cy %d idx %d bytes %d",
              bpp, cx, cy, idx, bytes);

    if ((idx < 0) || (idx >= 64))
    {
        LOG(LOG_LEVEL_ERROR, "lfreerdp_cache_brush: error idx %d", idx);
        return;
    }

    if ((bpp != 1) || (cx != 8) || (cy != 8))
    {
        LOG(LOG_LEVEL_ERROR, "lfreerdp_cache_brush: error unsupported brush "
            "bpp %d cx %d cy %d", bpp, cx, cy);
        return;
    }

    mod->brush_cache[idx].bpp = bpp;
    mod->brush_cache[idx].width = cx;
    mod->brush_cache[idx].height = cy;
    mod->brush_cache[idx].data = mod->brush_cache[idx].b8x8;

    if (bytes > 8)
    {
        bytes = 8;
    }

    g_memset(mod->brush_cache[idx].data, 0, 8);

    if (bytes > 0)
    {
        if (bytes > 8)
        {
            LOG_DEVEL(LOG_LEVEL_INFO, "lfreerdp_cache_brush: bytes too big %d", bytes);
            bytes = 8;
        }

        g_memcpy(mod->brush_cache[idx].data, cache_brush_order->data, bytes);
    }

    LOG_DEVEL(LOG_LEVEL_DEBUG, "lfreerdp_cache_brush: out bpp %d cx %d cy %d idx %d bytes %d",
              bpp, cx, cy, idx, bytes);

    free(cache_brush_order->data);
    cache_brush_order->data = 0;

}

/******************************************************************************/
static void
lfreerdp_pointer_position(rdpContext *context,
                          POINTER_POSITION_UPDATE *pointer_position)
{
    LOG_DEVEL(LOG_LEVEL_DEBUG, "lfreerdp_pointer_position: - no code here");
}

/******************************************************************************/
static void
lfreerdp_pointer_system(rdpContext *context,
                        POINTER_SYSTEM_UPDATE *pointer_system)
{
    LOG_DEVEL(LOG_LEVEL_DEBUG, "lfreerdp_pointer_system: - no code here type value = %d", pointer_system->type);
}

/******************************************************************************/
static void
lfreerdp_pointer_color(rdpContext *context,
                       POINTER_COLOR_UPDATE *pointer_color)
{
    LOG_DEVEL(LOG_LEVEL_DEBUG, "lfreerdp_pointer_color: - no code here");
}

/******************************************************************************/
static int
lfreerdp_get_pixel(void *bits, int width, int height, int bpp,
                   int delta, int x, int y)
{
    int start;
    int shift;
    int pixel;
    tui8 *src8;

    if (bpp == 1)
    {
        src8 = (tui8 *)bits;
        start = (y * delta) + x / 8;
        shift = x % 8;
        pixel = (src8[start] & (0x80 >> shift)) != 0;
        return pixel ? 0xffffff : 0;
    }
    else if (bpp == 32)
    {
        src8 = (tui8 *)bits;
        src8 += y * delta + x * 4;
        pixel = ((int *)(src8))[0];
        return pixel;
    }
    else
    {
        LOG(LOG_LEVEL_WARNING, "lfreerdp_get_pixel: unknown bpp %d", bpp);
    }

    return 0;
}

/******************************************************************************/
static int
lfreerdp_set_pixel(int pixel, void *bits, int width, int height, int bpp,
                   int delta, int x, int y)
{
    tui8 *dst8;
    int start;
    int shift;

    if (bpp == 1)
    {
        dst8 = (tui8 *)bits;
        start = (y * delta) + x / 8;
        shift = x % 8;

        if (pixel)
        {
            dst8[start] = dst8[start] | (0x80 >> shift);
        }
        else
        {
            dst8[start] = dst8[start] & ~(0x80 >> shift);
        }
    }
    else if (bpp == 24)
    {
        dst8 = (tui8 *)bits;
        dst8 += y * delta + x * 3;
        dst8[0] = (pixel >> 0) & 0xff;
        dst8[1] = (pixel >> 8) & 0xff;
        dst8[2] = (pixel >> 16) & 0xff;
    }
    else if (bpp == 32)
    {
        dst8 = (tui8 *)bits;
        dst8 += y * delta + x * 4;
        ((int *)(dst8))[0] = pixel;
    }
    else
    {
        LOG(LOG_LEVEL_WARNING, "lfreerdp_set_pixel: unknown bpp %d", bpp);
    }

    return 0;
}

/******************************************************************************/
static int
lfreerdp_convert_color_image(void *dst, int dst_width, int dst_height,
                             int dst_bpp, int dst_delta,
                             void *src, int src_width, int src_height,
                             int src_bpp, int src_delta)
{
    int i;
    int j;
    int pixel;

    for (j = 0; j < dst_height; j++)
    {
        for (i = 0; i < dst_width; i++)
        {
            pixel = lfreerdp_get_pixel(src, src_width, src_height, src_bpp,
                                       src_delta, i, j);
            lfreerdp_set_pixel(pixel, dst, dst_width, dst_height, dst_bpp,
                               dst_delta, i, j);
        }
    }

    return 0;
}

/******************************************************************************/
static void
lfreerdp_pointer_new(rdpContext *context,
                     POINTER_NEW_UPDATE *pointer_new)
{
    struct mod *mod;
    int index;
    int bytes_per_pixel;
    int bits_per_pixel;
    tui8 *dst;
    tui8 *src;

    mod = ((struct mod_context *)context)->modi;
    LOG_DEVEL(LOG_LEVEL_DEBUG, "lfreerdp_pointer_new:");
    LOG_DEVEL(LOG_LEVEL_DEBUG, "  bpp %d", pointer_new->xorBpp);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "  width %d height %d", pointer_new->colorPtrAttr.width,
              pointer_new->colorPtrAttr.height);

    LOG_DEVEL(LOG_LEVEL_DEBUG, "  lengthXorMask %d lengthAndMask %d",
              pointer_new->colorPtrAttr.lengthXorMask,
              pointer_new->colorPtrAttr.lengthAndMask);

    index = pointer_new->colorPtrAttr.cacheIndex;

    if (index >= 32)
    {
        LOG(LOG_LEVEL_ERROR, "lfreerdp_pointer_new: pointer index too big");
        return ;
    }

    if (pointer_new->xorBpp == 1 &&
            pointer_new->colorPtrAttr.width == 32 &&
            pointer_new->colorPtrAttr.height == 32)
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "lfreerdp_pointer_new:");
        mod->pointer_cache[index].hotx = pointer_new->colorPtrAttr.xPos;
        mod->pointer_cache[index].hoty = pointer_new->colorPtrAttr.yPos;
        mod->pointer_cache[index].bpp = 0;
        dst = (tui8 *)(mod->pointer_cache[index].data);
        dst += 32 * 32 * 3 - 32 * 3;
        src = pointer_new->colorPtrAttr.xorMaskData;
        lfreerdp_convert_color_image(dst, 32, 32, 24, 32 * -3,
                                     src, 32, 32, 1, 32 / 8);
        dst = (tui8 *)(mod->pointer_cache[index].mask);
        dst += ( 32 * 32 / 8) - (32 / 8);
        src = pointer_new->colorPtrAttr.andMaskData;
        lfreerdp_convert_color_image(dst, 32, 32, 1, 32 / -8,
                                     src, 32, 32, 1, 32 / 8);
    }
    else if (pointer_new->xorBpp >= 8 &&
             pointer_new->colorPtrAttr.width == 32 &&
             pointer_new->colorPtrAttr.height == 32)
    {
        bytes_per_pixel = (pointer_new->xorBpp + 7) / 8;
        bits_per_pixel = pointer_new->xorBpp;
        LOG_DEVEL(LOG_LEVEL_DEBUG, "lfreerdp_pointer_new: bpp %d Bpp %d", bits_per_pixel,
                  bytes_per_pixel);
        mod->pointer_cache[index].hotx = pointer_new->colorPtrAttr.xPos;
        mod->pointer_cache[index].hoty = pointer_new->colorPtrAttr.yPos;
        mod->pointer_cache[index].bpp = bits_per_pixel;
        memcpy(mod->pointer_cache[index].data,
               pointer_new->colorPtrAttr.xorMaskData,
               32 * 32 * bytes_per_pixel);
        memcpy(mod->pointer_cache[index].mask,
               pointer_new->colorPtrAttr.andMaskData,
               32 * (32 / 8));
    }
    else
    {
        LOG(LOG_LEVEL_WARNING, "lfreerdp_pointer_new: error bpp %d width %d height %d index: %d",
            pointer_new->xorBpp, pointer_new->colorPtrAttr.width,
            pointer_new->colorPtrAttr.height, index);
    }

    mod->server_set_pointer_ex(mod, mod->pointer_cache[index].hotx,
                               mod->pointer_cache[index].hoty,
                               mod->pointer_cache[index].data,
                               mod->pointer_cache[index].mask,
                               mod->pointer_cache[index].bpp);

    free(pointer_new->colorPtrAttr.xorMaskData);
    pointer_new->colorPtrAttr.xorMaskData = 0;
    free(pointer_new->colorPtrAttr.andMaskData);
    pointer_new->colorPtrAttr.andMaskData = 0;

}

/******************************************************************************/
static void
lfreerdp_pointer_cached(rdpContext *context,
                        POINTER_CACHED_UPDATE *pointer_cached)
{
    struct mod *mod;
    int index;

    mod = ((struct mod_context *)context)->modi;
    index = pointer_cached->cacheIndex;
    LOG_DEVEL(LOG_LEVEL_DEBUG, "lfreerdp_pointer_cached:%d", index);
    mod->server_set_pointer_ex(mod, mod->pointer_cache[index].hotx,
                               mod->pointer_cache[index].hoty,
                               mod->pointer_cache[index].data,
                               mod->pointer_cache[index].mask,
                               mod->pointer_cache[index].bpp);
}

/******************************************************************************/
static void
lfreerdp_polygon_cb(rdpContext *context, POLYGON_CB_ORDER *polygon_cb)
{
    LOG_DEVEL(LOG_LEVEL_DEBUG, "lfreerdp_polygon_cb called:- not supported!!!!!!!!!!!!!!!!!!!!");
}

/******************************************************************************/
static void
lfreerdp_polygon_sc(rdpContext *context, POLYGON_SC_ORDER *polygon_sc)
{
    struct mod *mod;
    int i;
    struct
    {
        short x, y;
    } points[4];
    int fgcolor;
    int server_bpp, client_bpp;

    mod = ((struct mod_context *)context)->modi;
    LOG_DEVEL(LOG_LEVEL_DEBUG, "lfreerdp_polygon_sc :%d(points) %d(color) %d(fillmode) "
              "%d(bRop) %d(cbData) %d(x) %d(y)",
              polygon_sc->nDeltaEntries, polygon_sc->brushColor,
              polygon_sc->fillMode, polygon_sc->bRop2,
              polygon_sc->cbData, polygon_sc->xStart,
              polygon_sc->yStart);

    if (polygon_sc->nDeltaEntries == 3)
    {
        server_bpp = mod->inst->settings->color_depth;
        client_bpp = mod->bpp;

        points[0].x = polygon_sc->xStart;
        points[0].y = polygon_sc->yStart;

        for (i = 0; i < polygon_sc->nDeltaEntries; i++)
        {
            points[i + 1].x = 0; // polygon_sc->points[i].x;
            points[i + 1].y = 0; // polygon_sc->points[i].y;
        }

        fgcolor = convert_color(server_bpp, client_bpp,
                                polygon_sc->brushColor, mod->colormap);

        mod->server_set_opcode(mod, polygon_sc->bRop2);
        mod->server_set_bgcolor(mod, 255);
        mod->server_set_fgcolor(mod, fgcolor);
        mod->server_set_pen(mod, 1, 1); // style, width
        // TODO replace with correct brush; this is a workaround
        // This workaround handles the text cursor in microsoft word.
        mod->server_draw_line(mod, polygon_sc->xStart, polygon_sc->yStart, polygon_sc->xStart, polygon_sc->yStart + points[2].y);
        //        mod->server_fill_rect(mod, points[0].x, points[0].y,
        //                         points[0].x-points[3].x, points[0].y-points[2].y);
        //      mod->server_set_brush(mod,); // howto use this on our indata??
        mod->server_set_opcode(mod, 0xcc);
    }
    else
    {
        LOG(LOG_LEVEL_WARNING, "Not handled number of points in lfreerdp_polygon_sc");
    }
}

/******************************************************************************/
static void
lfreerdp_synchronize(rdpContext *context)
{
    /* Uncomment these two lines when needed */
#if 0
    struct mod *mod;
    mod = ((struct mod_context *)context)->modi;
#endif
    LOG_DEVEL(LOG_LEVEL_DEBUG, "lfreerdp_synchronize received - not handled");
}

/******************************************************************************/
static boolean
lfreerdp_pre_connect(freerdp *instance)
{
    struct mod *mod;
    int index;
    int error;
    int num_chans;
    int target_chan;
    int ch_flags;
    char ch_name[256];
    const char *ch_names[MAX_STATIC_CHANNELS];
    char *dst_ch_name;

    LOG_DEVEL(LOG_LEVEL_INFO, "lfreerdp_pre_connect:");
    mod = ((struct mod_context *)(instance->context))->modi;
    verifyColorMap(mod);

    num_chans = mod->server_get_channel_count(mod);
    if (num_chans < 0)
    {
        LOG(LOG_LEVEL_ERROR, "lfreerdp_pre_connect: Can't get channel count");
        num_chans = 0;
    }

    target_chan = 0;
    for (index = 0 ; index < num_chans; ++index)
    {
        error = mod->server_query_channel(mod, index, ch_name, &ch_flags);
        if (error == 0)
        {
            LOG_DEVEL(LOG_LEVEL_DEBUG, "lfreerdp_pre_connect: "
                      "got channel [%s], id [%d], flags [0x%8.8x]",
                      ch_name, index, ch_flags);

            if (g_strcmp(ch_name, DRDYNVC_SVC_CHANNEL_NAME) == 0)
            {
                /* xrdp currently reserves dynamic channels for its
                 * exclusive use (e.g. for GFX support) */
                LOG(LOG_LEVEL_INFO, "Channel '%s' not passed to module",
                    ch_name);
            }
            else if (target_chan < MAX_STATIC_CHANNELS)
            {
                dst_ch_name = instance->settings->channels[target_chan].name;
                ch_names[target_chan] = dst_ch_name;
                g_memset(dst_ch_name, 0, CHANNEL_NAME_LEN + 1);
                g_snprintf(dst_ch_name, CHANNEL_NAME_LEN + 1, "%s", ch_name);
                instance->settings->channels[target_chan].options = ch_flags;
                ++target_chan;
            }
        }
    }

    g_strnjoin(ch_name, sizeof(ch_name), ",", ch_names, target_chan);
    LOG(LOG_LEVEL_INFO, "Static channels (from %d) passed to module : %s",
        num_chans, ch_name);

    instance->settings->num_channels = target_chan;

    instance->settings->offscreen_bitmap_cache = 0;
    instance->settings->draw_nine_grid = 0;

    instance->settings->glyph_cache = true;
    /* GLYPH_SUPPORT_FULL and GLYPH_SUPPORT_PARTIAL seem to be the same */
    /* disabled as workaround for corrupted display like black bars left of cmd with W2K8 */
    /* instance->settings->glyphSupportLevel = GLYPH_SUPPORT_FULL; */
    instance->settings->glyphSupportLevel = GLYPH_SUPPORT_NONE;

    instance->settings->order_support[NEG_DSTBLT_INDEX] = 1; /* 0x00 */
    instance->settings->order_support[NEG_PATBLT_INDEX] = 1;
    instance->settings->order_support[NEG_SCRBLT_INDEX] = 1;
    instance->settings->order_support[NEG_MEMBLT_INDEX] = 1;
    instance->settings->order_support[NEG_MEM3BLT_INDEX] = 0;
    instance->settings->order_support[NEG_ATEXTOUT_INDEX] = 0;
    instance->settings->order_support[NEG_AEXTTEXTOUT_INDEX] = 0;
    instance->settings->order_support[NEG_DRAWNINEGRID_INDEX] = 0;
    instance->settings->order_support[NEG_LINETO_INDEX] = 1; /* 0x08 */
    instance->settings->order_support[NEG_MULTI_DRAWNINEGRID_INDEX] = 0;
    instance->settings->order_support[NEG_OPAQUE_RECT_INDEX] = 1;
    instance->settings->order_support[NEG_SAVEBITMAP_INDEX] = 0;
    instance->settings->order_support[NEG_WTEXTOUT_INDEX] = 0;
    instance->settings->order_support[NEG_MEMBLT_V2_INDEX] = 1;
    instance->settings->order_support[NEG_MEM3BLT_V2_INDEX] = 0;
    instance->settings->order_support[NEG_MULTIDSTBLT_INDEX] = 0;
    instance->settings->order_support[NEG_MULTIPATBLT_INDEX] = 0; /* 0x10 */
    instance->settings->order_support[NEG_MULTISCRBLT_INDEX] = 0;
    instance->settings->order_support[NEG_MULTIOPAQUERECT_INDEX] = 0;
    instance->settings->order_support[NEG_FAST_INDEX_INDEX] = 0;
    instance->settings->order_support[NEG_POLYGON_SC_INDEX] = 0;
    instance->settings->order_support[NEG_POLYGON_CB_INDEX] = 0;
    instance->settings->order_support[NEG_POLYLINE_INDEX] = 0;
    /* 0x17 missing */
    instance->settings->order_support[NEG_FAST_GLYPH_INDEX] = 0; /* 0x18 */
    instance->settings->order_support[NEG_ELLIPSE_SC_INDEX] = 0;
    instance->settings->order_support[NEG_ELLIPSE_CB_INDEX] = 0;
    /* disabled as workaround for corrupted display like black bars left of cmd with W2K8 */
    /* instance->settings->order_support[NEG_GLYPH_INDEX_INDEX] = 1; */
    instance->settings->order_support[NEG_GLYPH_INDEX_INDEX] = 0;

    instance->settings->order_support[NEG_GLYPH_WEXTTEXTOUT_INDEX] = 0;
    instance->settings->order_support[NEG_GLYPH_WLONGTEXTOUT_INDEX] = 0;
    instance->settings->order_support[NEG_GLYPH_WLONGEXTTEXTOUT_INDEX] = 0;
    /* 0x1F missing*/

    instance->settings->bitmap_cache = 1;
    instance->settings->bitmapCacheV2NumCells = 3; // 5;
    instance->settings->bitmapCacheV2CellInfo[0].numEntries = 600; // 0x78;
    instance->settings->bitmapCacheV2CellInfo[0].persistent = 0;
    instance->settings->bitmapCacheV2CellInfo[1].numEntries = 600; //0x78; // 600;
    instance->settings->bitmapCacheV2CellInfo[1].persistent = 0;
    instance->settings->bitmapCacheV2CellInfo[2].numEntries = 2048; //0x150; // 2048;
    instance->settings->bitmapCacheV2CellInfo[2].persistent = 0;
    instance->settings->bitmapCacheV2CellInfo[3].numEntries = 4096; // 4096;
    instance->settings->bitmapCacheV2CellInfo[3].persistent = 0;
    instance->settings->bitmapCacheV2CellInfo[4].numEntries = 2048; // 2048;
    instance->settings->bitmapCacheV2CellInfo[4].persistent = 0;

    instance->settings->bitmap_cache_v3 = 1;

    instance->settings->username = g_strdup(mod->username);
    instance->settings->password = g_strdup(mod->password);
    instance->settings->domain = g_strdup(mod->domain);

    if (mod->client_info.rail_enable && (mod->client_info.rail_support_level > 0))
    {
        LOG_DEVEL(LOG_LEVEL_INFO, "Railsupport !!!!!!!!!!!!!!!!!!");
        instance->settings->remote_app = 1;
        instance->settings->rail_langbar_supported = 1;
        instance->settings->workarea = 1;
        instance->settings->performance_flags = PERF_DISABLE_WALLPAPER | PERF_DISABLE_FULLWINDOWDRAG;
        instance->settings->num_icon_caches = mod->client_info.wnd_num_icon_caches;
        instance->settings->num_icon_cache_entries = mod->client_info.wnd_num_icon_cache_entries;


    }
    else
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "Special PerformanceFlags changed");
        instance->settings->performance_flags = PERF_DISABLE_WALLPAPER |
                                                PERF_DISABLE_FULLWINDOWDRAG | PERF_DISABLE_MENUANIMATIONS |
                                                PERF_DISABLE_THEMING;
        // | PERF_DISABLE_CURSOR_SHADOW | PERF_DISABLE_CURSORSETTINGS;
    }

    /* Allow users or administrators to configure the mstsc experience settings. #1903 */

    if ((mod->allow_client_experiencesettings == 1) &&
            (mod->client_info.mcs_connection_type == CONNECTION_TYPE_AUTODETECT))
    {
        /* auto-detect not yet supported - use default performance settings */
    }
    else if (mod->allow_client_experiencesettings == 1)
    {
        instance->settings->performance_flags =
            (mod->client_info.rdp5_performanceflags &
             /* Mask to avoid accepting invalid flags. */
             (PERF_DISABLE_WALLPAPER |
              PERF_DISABLE_FULLWINDOWDRAG |
              PERF_DISABLE_MENUANIMATIONS |
              PERF_DISABLE_THEMING |
              PERF_DISABLE_CURSOR_SHADOW |
              PERF_DISABLE_CURSORSETTINGS |
              PERF_ENABLE_FONT_SMOOTHING |
              PERF_ENABLE_DESKTOP_COMPOSITION));

        LOG(LOG_LEVEL_DEBUG, "RDP client experience settings, "
            "rdp5_performance_flags:[0x%08x], "
            "masked performance_flags:[0x%08x]",
            mod->client_info.rdp5_performanceflags,
            instance->settings->performance_flags);

        if (mod->client_info.rail_enable &&
                (mod->client_info.rail_support_level > 0))
        {
            instance->settings->performance_flags |= (PERF_DISABLE_WALLPAPER |
                PERF_DISABLE_FULLWINDOWDRAG);
            LOG(LOG_LEVEL_DEBUG, "Add in performance setting for Railsupport:"
                "[0x%08x]", PERF_DISABLE_WALLPAPER |
                PERF_DISABLE_FULLWINDOWDRAG);
        }
    }

    LOG(LOG_LEVEL_DEBUG, "before overriding performance_flags:[0x%08x]",
        instance->settings->performance_flags);
    LOG(LOG_LEVEL_DEBUG, "perf_settings_override_mask:[0x%08x], "
        "perf_settings_values_mask:[0x%08x]",
        mod->perf_settings_override_mask,
        mod->perf_settings_values_mask);

    /* Clear bits for any overridden performance settings */
    instance->settings->performance_flags &= ~mod->perf_settings_override_mask;

    /* Add in overridden performance settings */
    instance->settings->performance_flags |= mod->perf_settings_values_mask;

    LOG(LOG_LEVEL_DEBUG, "final performance_flags:[0x%08x]",
        instance->settings->performance_flags);

    instance->settings->compression = 0;
    instance->settings->ignore_certificate = 1;

    // Multi Monitor Settings
    const struct display_size_description *display_sizes =
            &mod->client_info.display_sizes;
    instance->settings->num_monitors = display_sizes->monitorCount;

    for (index = 0; index < display_sizes->monitorCount; index++)
    {
        instance->settings->monitors[index].x = display_sizes->minfo[index].left;
        instance->settings->monitors[index].y = display_sizes->minfo[index].top;
        instance->settings->monitors[index].width = display_sizes->minfo[index].right;
        instance->settings->monitors[index].height = display_sizes->minfo[index].bottom;
        instance->settings->monitors[index].is_primary = display_sizes->minfo[index].is_primary;
    }

    instance->update->BeginPaint = lfreerdp_begin_paint;
    instance->update->EndPaint = lfreerdp_end_paint;
    instance->update->SetBounds = lfreerdp_set_bounds;
    instance->update->BitmapUpdate = lfreerdp_bitmap_update;
    instance->update->Synchronize = lfreerdp_synchronize;
    instance->update->primary->DstBlt = lfreerdp_dst_blt;
    instance->update->primary->PatBlt = lfreerdp_pat_blt;
    instance->update->primary->ScrBlt = lfreerdp_scr_blt;
    instance->update->primary->OpaqueRect = lfreerdp_opaque_rect;
    instance->update->primary->MemBlt = lfreerdp_mem_blt;
    instance->update->primary->GlyphIndex = lfreerdp_glyph_index;
    instance->update->primary->LineTo = lfreerdp_line_to;
    instance->update->primary->PolygonSC = lfreerdp_polygon_sc ;
    instance->update->primary->PolygonCB = lfreerdp_polygon_cb;
    instance->update->secondary->CacheBitmap = lfreerdp_cache_bitmap;
    instance->update->secondary->CacheBitmapV2 = lfreerdp_cache_bitmapV2;
    instance->update->secondary->CacheGlyph = lfreerdp_cache_glyph;
    instance->update->secondary->CacheBrush = lfreerdp_cache_brush;

    instance->update->pointer->PointerPosition = lfreerdp_pointer_position;
    instance->update->pointer->PointerSystem = lfreerdp_pointer_system;
    instance->update->pointer->PointerColor = lfreerdp_pointer_color;
    instance->update->pointer->PointerNew = lfreerdp_pointer_new;
    instance->update->pointer->PointerCached = lfreerdp_pointer_cached;

    if ((mod->username[0] != 0) && (mod->password[0] != 0))
    {
        /* since we have username and password, we can try nla */
        instance->settings->nla_security = 1;
    }
    else
    {
        instance->settings->nla_security = 0;
    }

    return 1;
}

/*****************************************************************************/
void
lrail_WindowCreate(rdpContext *context, WINDOW_ORDER_INFO *orderInfo,
                   WINDOW_STATE_ORDER *window_state)
{
    int index;
    struct mod *mod;
    struct rail_window_state_order wso;
    UNICONV *uniconv;

    LOG_DEVEL(LOG_LEVEL_TRACE, "lrail_WindowCreate:");
    uniconv = freerdp_uniconv_new();
    mod = ((struct mod_context *)context)->modi;
    memset(&wso, 0, sizeof(wso));
    /* copy the window state order */
    wso.owner_window_id = window_state->ownerWindowId;
    wso.style = window_state->style;
    wso.extended_style = window_state->extendedStyle;
    wso.show_state = window_state->showState;

    if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_TITLE)
    {
        wso.title_info = freerdp_uniconv_in(uniconv,
                                            window_state->titleInfo.string, window_state->titleInfo.length);
    }

    LOG_DEVEL(LOG_LEVEL_DEBUG, "lrail_WindowCreate: %s", wso.title_info);
    wso.client_offset_x = window_state->clientOffsetX;
    wso.client_offset_y = window_state->clientOffsetY;
    wso.client_area_width = window_state->clientAreaWidth;
    wso.client_area_height = window_state->clientAreaHeight;
    wso.rp_content = window_state->RPContent;
    wso.root_parent_handle = window_state->rootParentHandle;
    wso.window_offset_x = window_state->windowOffsetX;
    wso.window_offset_y = window_state->windowOffsetY;
    wso.window_client_delta_x = window_state->windowClientDeltaX;
    wso.window_client_delta_y = window_state->windowClientDeltaY;
    wso.window_width = window_state->windowWidth;
    wso.window_height = window_state->windowHeight;
    wso.num_window_rects = window_state->numWindowRects;

    if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_RECTS)
    {
        wso.window_rects = (struct rail_window_rect *)
                           g_malloc(sizeof(struct rail_window_rect) * wso.num_window_rects, 0);

        for (index = 0; index < wso.num_window_rects; index++)
        {
            wso.window_rects[index].left = window_state->windowRects[index].left;
            wso.window_rects[index].top = window_state->windowRects[index].top;
            wso.window_rects[index].right = window_state->windowRects[index].right;
            wso.window_rects[index].bottom = window_state->windowRects[index].bottom;
        }
    }

    wso.visible_offset_x = window_state->visibleOffsetX;
    wso.visible_offset_y = window_state->visibleOffsetY;
    wso.num_visibility_rects = window_state->numVisibilityRects;

    if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_VISIBILITY)
    {
        wso.visibility_rects = (struct rail_window_rect *)
                               g_malloc(sizeof(struct rail_window_rect) * wso.num_visibility_rects, 0);

        for (index = 0; index < wso.num_visibility_rects; index++)
        {
            wso.visibility_rects[index].left = window_state->visibilityRects[index].left;
            wso.visibility_rects[index].top = window_state->visibilityRects[index].top;
            wso.visibility_rects[index].right = window_state->visibilityRects[index].right;
            wso.visibility_rects[index].bottom = window_state->visibilityRects[index].bottom;
        }
    }

    mod->server_window_new_update(mod, orderInfo->windowId, &wso,
                                  orderInfo->fieldFlags);

    free(wso.title_info);
    g_free(wso.window_rects);
    g_free(wso.visibility_rects);
}

/*****************************************************************************/
void
lrail_WindowUpdate(rdpContext *context, WINDOW_ORDER_INFO *orderInfo,
                   WINDOW_STATE_ORDER *window_state)
{
    LOG_DEVEL(LOG_LEVEL_TRACE, "lrail_WindowUpdate:");
    lrail_WindowCreate(context, orderInfo, window_state);
}

/*****************************************************************************/
void
lrail_WindowDelete(rdpContext *context, WINDOW_ORDER_INFO *orderInfo)
{
    struct mod *mod;

    LOG_DEVEL(LOG_LEVEL_TRACE, "lrail_WindowDelete:");
    mod = ((struct mod_context *)context)->modi;
    mod->server_window_delete(mod, orderInfo->windowId);
}

/*****************************************************************************/
void
lrail_WindowIcon(rdpContext *context, WINDOW_ORDER_INFO *orderInfo,
                 WINDOW_ICON_ORDER *window_icon)
{
    struct mod *mod;
    struct rail_icon_info rii;

    LOG_DEVEL(LOG_LEVEL_TRACE, "lrail_WindowIcon:");
    mod = ((struct mod_context *)context)->modi;
    memset(&rii, 0, sizeof(rii));
    rii.bpp = window_icon->iconInfo->bpp;
    rii.width = window_icon->iconInfo->width;
    rii.height = window_icon->iconInfo->height;
    rii.cmap_bytes = window_icon->iconInfo->cbColorTable;
    rii.mask_bytes = window_icon->iconInfo->cbBitsMask;
    rii.data_bytes = window_icon->iconInfo->cbBitsColor;
    rii.mask = (char *)(window_icon->iconInfo->bitsMask);
    rii.cmap = (char *)(window_icon->iconInfo->colorTable);
    rii.data = (char *)(window_icon->iconInfo->bitsColor);
    mod->server_window_icon(mod, orderInfo->windowId,
                            window_icon->iconInfo->cacheEntry,
                            window_icon->iconInfo->cacheId, &rii,
                            orderInfo->fieldFlags);
}

/*****************************************************************************/
void
lrail_WindowCachedIcon(rdpContext *context, WINDOW_ORDER_INFO *orderInfo,
                       WINDOW_CACHED_ICON_ORDER *window_cached_icon)
{
    struct mod *mod;

    LOG_DEVEL(LOG_LEVEL_TRACE, "lrail_WindowCachedIcon:");
    mod = ((struct mod_context *)context)->modi;
    mod->server_window_cached_icon(mod, orderInfo->windowId,
                                   window_cached_icon->cachedIcon.cacheEntry,
                                   window_cached_icon->cachedIcon.cacheId,
                                   orderInfo->fieldFlags);
}

/*****************************************************************************/
void
lrail_NotifyIconCreate(rdpContext *context, WINDOW_ORDER_INFO *orderInfo,
                       NOTIFY_ICON_STATE_ORDER *notify_icon_state)
{
    struct mod *mod;
    struct rail_notify_state_order rnso;
    UNICONV *uniconv;

    LOG_DEVEL(LOG_LEVEL_TRACE, "lrail_NotifyIconCreate:");
    uniconv = freerdp_uniconv_new();
    mod = ((struct mod_context *)context)->modi;

    memset(&rnso, 0, sizeof(rnso));
    rnso.version = notify_icon_state->version;

    if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_NOTIFY_TIP)
    {
        rnso.tool_tip = freerdp_uniconv_in(uniconv,
                                           notify_icon_state->toolTip.string, notify_icon_state->toolTip.length);
    }

    if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_NOTIFY_INFO_TIP)
    {
        rnso.infotip.timeout = notify_icon_state->infoTip.timeout;
        rnso.infotip.flags = notify_icon_state->infoTip.flags;
        rnso.infotip.text = freerdp_uniconv_in(uniconv,
                                               notify_icon_state->infoTip.text.string,
                                               notify_icon_state->infoTip.text.length);
        rnso.infotip.title = freerdp_uniconv_in(uniconv,
                                                notify_icon_state->infoTip.title.string,
                                                notify_icon_state->infoTip.title.length);
    }

    rnso.state = notify_icon_state->state;
    rnso.icon_cache_entry = notify_icon_state->icon.cacheEntry;
    rnso.icon_cache_id = notify_icon_state->icon.cacheId;

    rnso.icon_info.bpp = notify_icon_state->icon.bpp;
    rnso.icon_info.width = notify_icon_state->icon.width;
    rnso.icon_info.height = notify_icon_state->icon.height;
    rnso.icon_info.cmap_bytes = notify_icon_state->icon.cbColorTable;
    rnso.icon_info.mask_bytes = notify_icon_state->icon.cbBitsMask;
    rnso.icon_info.data_bytes = notify_icon_state->icon.cbBitsColor;
    rnso.icon_info.mask = (char *)(notify_icon_state->icon.bitsMask);
    rnso.icon_info.cmap = (char *)(notify_icon_state->icon.colorTable);
    rnso.icon_info.data = (char *)(notify_icon_state->icon.bitsColor);

    mod->server_notify_new_update(mod, orderInfo->windowId,
                                  orderInfo->notifyIconId,
                                  &rnso, orderInfo->fieldFlags);

    free(rnso.tool_tip);
    free(rnso.infotip.text);
    free(rnso.infotip.title);
}

/*****************************************************************************/
void
lrail_NotifyIconUpdate(rdpContext *context, WINDOW_ORDER_INFO *orderInfo,
                       NOTIFY_ICON_STATE_ORDER *notify_icon_state)
{
    LOG_DEVEL(LOG_LEVEL_TRACE, "lrail_NotifyIconUpdate:");
    lrail_NotifyIconCreate(context, orderInfo, notify_icon_state);
}

/*****************************************************************************/
void
lrail_NotifyIconDelete(rdpContext *context, WINDOW_ORDER_INFO *orderInfo)
{
    struct mod *mod;

    LOG_DEVEL(LOG_LEVEL_TRACE, "lrail_NotifyIconDelete:");
    mod = ((struct mod_context *)context)->modi;
    mod->server_notify_delete(mod, orderInfo->windowId,
                              orderInfo->notifyIconId);
}

/*****************************************************************************/
void
lrail_MonitoredDesktop(rdpContext *context, WINDOW_ORDER_INFO *orderInfo,
                       MONITORED_DESKTOP_ORDER *monitored_desktop)
{
    int index;
    struct mod *mod;
    struct rail_monitored_desktop_order rmdo;

    LOG_DEVEL(LOG_LEVEL_TRACE, "lrail_MonitoredDesktop:");
    mod = ((struct mod_context *)context)->modi;
    memset(&rmdo, 0, sizeof(rmdo));
    rmdo.active_window_id = monitored_desktop->activeWindowId;
    rmdo.num_window_ids = monitored_desktop->numWindowIds;

    if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_DESKTOP_ZORDER)
    {
        if (rmdo.num_window_ids > 0)
        {
            rmdo.window_ids = (int *)g_malloc(sizeof(int) * rmdo.num_window_ids, 0);

            for (index = 0; index < rmdo.num_window_ids; index++)
            {
                rmdo.window_ids[index] = monitored_desktop->windowIds[index];
            }
        }
    }

    mod->server_monitored_desktop(mod, &rmdo, orderInfo->fieldFlags);
    g_free(rmdo.window_ids);
}

/*****************************************************************************/
void
lrail_NonMonitoredDesktop(rdpContext *context, WINDOW_ORDER_INFO *orderInfo)
{
    struct mod *mod;
    struct rail_monitored_desktop_order rmdo;

    LOG_DEVEL(LOG_LEVEL_TRACE, "lrail_NonMonitoredDesktop:");
    mod = ((struct mod_context *)context)->modi;
    memset(&rmdo, 0, sizeof(rmdo));
    mod->server_monitored_desktop(mod, &rmdo, orderInfo->fieldFlags);
}

/******************************************************************************/
static boolean
lfreerdp_post_connect(freerdp *instance)
{
    struct mod *mod;

    LOG_DEVEL(LOG_LEVEL_TRACE, "lfreerdp_post_connect:");
    mod = ((struct mod_context *)(instance->context))->modi;
    g_memset(mod->password, 0, sizeof(mod->password));

    mod->inst->update->window->WindowCreate = lrail_WindowCreate;
    mod->inst->update->window->WindowUpdate = lrail_WindowUpdate;
    mod->inst->update->window->WindowDelete = lrail_WindowDelete;
    mod->inst->update->window->WindowIcon = lrail_WindowIcon;
    mod->inst->update->window->WindowCachedIcon = lrail_WindowCachedIcon;
    mod->inst->update->window->NotifyIconCreate = lrail_NotifyIconCreate;
    mod->inst->update->window->NotifyIconUpdate = lrail_NotifyIconUpdate;
    mod->inst->update->window->NotifyIconDelete = lrail_NotifyIconDelete;
    mod->inst->update->window->MonitoredDesktop = lrail_MonitoredDesktop;
    mod->inst->update->window->NonMonitoredDesktop = lrail_NonMonitoredDesktop;

    return 1;
}

/******************************************************************************/
static void
lfreerdp_context_new(freerdp *instance, rdpContext *context)
{
    LOG_DEVEL(LOG_LEVEL_INFO, "lfreerdp_context_new: %p", context);
}

/******************************************************************************/
static void
lfreerdp_context_free(freerdp *instance, rdpContext *context)
{
    LOG_DEVEL(LOG_LEVEL_INFO, "lfreerdp_context_free: - no code here");
}

/******************************************************************************/
static int
lfreerdp_receive_channel_data(freerdp *instance, int channelId, uint8 *data,
                              int size, int flags, int total_size)
{
    struct mod *mod;
    int lchid;
    int index;
    int error;

    mod = ((struct mod_context *)(instance->context))->modi;
    lchid = -1;

    for (index = 0; index < instance->settings->num_channels; index++)
    {
        if (instance->settings->channels[index].channel_id == channelId)
        {
            lchid = index;
            break;
        }
    }

    if (lchid >= 0)
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "lfreerdp_receive_channel_data: server to client, chanid: %d", lchid);
        error = mod->server_send_to_channel(mod, lchid, (char *)data, size,
                                            total_size, flags);

        if (error != 0)
        {
            LOG(LOG_LEVEL_ERROR, "lfreerdp_receive_channel_data: error %d", error);
        }
    }
    else
    {
        LOG(LOG_LEVEL_ERROR, "lfreerdp_receive_channel_data: bad lchid");
    }

    return 0;
}

/******************************************************************************/
static boolean
lfreerdp_authenticate(freerdp *instance, char **username,
                      char **password, char **domain)
{
    LOG_DEVEL(LOG_LEVEL_DEBUG, "lfreerdp_authenticate: - no code here");
    return 1;
}

/******************************************************************************/
static boolean
lfreerdp_verify_certificate(freerdp *instance, char *subject, char *issuer,
                            char *fingerprint)
{
    LOG_DEVEL(LOG_LEVEL_DEBUG, "lfreerdp_verify_certificate: - no code here");
    return 1;
}

/******************************************************************************/
static int
lfreerdp_session_info(freerdp *instance, uint8 *data, int data_bytes)
{
    struct mod *mod;
    int error;

    LOG_DEVEL(LOG_LEVEL_TRACE, "lfreerdp_session_info:");
    error = 0;
    mod = ((struct mod_context *)(instance->context))->modi;
    if (mod != 0)
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "lfreerdp_session_info: mod->server_session_info %p",
                  mod->server_session_info);
        if (mod->server_session_info != 0)
        {
            error = mod->server_session_info(mod, (char *)data, data_bytes);
        }
    }
    return error;
}

/******************************************************************************/
tintptr EXPORT_CC
mod_init(void)
{
    struct mod *mod;
    modContext *lcon;

    LOG_DEVEL(LOG_LEVEL_TRACE, "mod_init:");
    mod = (struct mod *)g_malloc(sizeof(struct mod), 1);
    freerdp_get_version(&(mod->vmaj), &(mod->vmin), &(mod->vrev));
    LOG(LOG_LEVEL_INFO, "  FreeRDP version major %d minor %d revision %d",
        mod->vmaj, mod->vmin, mod->vrev);
    mod->size = sizeof(struct mod);
    mod->version = CURRENT_MOD_VER;
    mod->handle = (tintptr) mod;
    mod->mod_connect = lxrdp_connect;
    mod->mod_start = lxrdp_start;
    mod->mod_event = lxrdp_event;
    mod->mod_signal = lxrdp_signal;
    mod->mod_end = lxrdp_end;
    mod->mod_set_param = lxrdp_set_param;
    mod->mod_session_change = lxrdp_session_change;
    mod->mod_get_wait_objs = lxrdp_get_wait_objs;
    mod->mod_check_wait_objs = lxrdp_check_wait_objs;
    mod->mod_frame_ack = lxrdp_frame_ack;
    mod->mod_suppress_output = lxrdp_suppress_output;
    mod->mod_server_version_message = lxrdp_server_version_message;
    mod->mod_server_monitor_resize = lxrdp_server_monitor_resize;
    mod->mod_server_monitor_full_invalidate = lxrdp_server_monitor_full_invalidate;

    mod->inst = freerdp_new();
    mod->inst->PreConnect = lfreerdp_pre_connect;
    mod->inst->PostConnect = lfreerdp_post_connect;
    mod->inst->context_size = sizeof(modContext);
    mod->inst->ContextNew = lfreerdp_context_new;
    mod->inst->ContextFree = lfreerdp_context_free;
    mod->inst->ReceiveChannelData = lfreerdp_receive_channel_data;
    mod->inst->Authenticate = lfreerdp_authenticate;
    mod->inst->VerifyCertificate = lfreerdp_verify_certificate;
#if defined(VERSION_STRUCT_RDP_FREERDP)
#if VERSION_STRUCT_RDP_FREERDP > 0
    mod->inst->SessionInfo = lfreerdp_session_info;
#endif
#endif

    freerdp_context_new(mod->inst);

    lcon = (modContext *)(mod->inst->context);
    lcon->modi = mod;
    LOG_DEVEL(LOG_LEVEL_DEBUG, "mod_init: mod %p", mod);

    return (tintptr) mod;
}

/******************************************************************************/
int EXPORT_CC
mod_exit(tintptr handle)
{
    struct mod *mod = (struct mod *) handle;

    LOG_DEVEL(LOG_LEVEL_TRACE, "mod_exit:");

    if (mod == 0)
    {
        return 0;
    }

    if (mod->inst == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "mod_exit - null pointer for inst:");
        g_free(mod);
        return 0 ;
    }

    freerdp_disconnect(mod->inst);

    if ((mod->vmaj == 1) && (mod->vmin == 0) && (mod->vrev == 1))
    {
        /* this version has a bug with double free in freerdp_free */
    }
    else
    {
        freerdp_context_free(mod->inst);
    }

    freerdp_free(mod->inst);
    g_free(mod);
    return 0;
}
