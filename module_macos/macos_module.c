/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) 2026 Neutrinos Software Corporation
 * Some portions Classify(r)
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
 * @file macos_module.c
 * @brief xrdp module interface implementation for native macOS capture
 */

#include "macos_capture.h"

/**
 * Initialize module
 */
tintptr EXPORT_CC
mod_init(void)
{
    struct mod_macos* mod;

    LOG(LOG_LEVEL_INFO, "mod_init: macOS native capture module");

    mod = (struct mod_macos*)g_malloc(sizeof(struct mod_macos), 1);
    if (mod == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "mod_init: g_malloc failed");
        return 0;
    }

    mod->size = sizeof(struct mod_macos);
    mod->handle = (tbus)mod;
    mod->capture_context = NULL;
    mod->frame_buffer = NULL;
    mod->has_damage = 0;

    return (tintptr)mod;
}

/**
 * Exit module
 */
int EXPORT_CC
mod_exit(tintptr handle)
{
    struct mod_macos* mod = (struct mod_macos*)handle;

    LOG(LOG_LEVEL_INFO, "mod_exit");

    if (mod == NULL)
    {
        return 0;
    }

    if (mod->frame_buffer != NULL)
    {
        g_free(mod->frame_buffer);
    }

    g_free(mod);

    return 0;
}

/**
 * Start module with specified dimensions
 */
int EXPORT_CC
mod_start(tintptr handle, int w, int h, int bpp)
{
    struct mod_macos* mod = (struct mod_macos*)handle;

    LOG(LOG_LEVEL_INFO, "mod_start: %dx%d @ %dbpp", w, h, bpp);

    if (mod == NULL)
    {
        return 1;
    }

    mod->width = w;
    mod->height = h;
    mod->bpp = bpp;

    /* Allocate frame buffer */
    mod->frame_buffer_size = w * h * 4; /* Always use 32bpp internally */
    mod->frame_buffer = g_malloc(mod->frame_buffer_size, 0);
    if (mod->frame_buffer == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "mod_start: failed to allocate frame buffer");
        return 1;
    }

    return 0;
}

/**
 * Connect and start capture
 */
int EXPORT_CC
mod_connect(tintptr handle)
{
    struct mod_macos* mod = (struct mod_macos*)handle;
    int rv;

    LOG(LOG_LEVEL_INFO, "mod_connect");

    if (mod == NULL)
    {
        return 1;
    }

    /* Initialize capture */
    rv = macos_capture_init(mod);
    if (rv != 0)
    {
        LOG(LOG_LEVEL_ERROR, "mod_connect: macos_capture_init failed");
        return rv;
    }

    /* Start capture */
    rv = macos_capture_start(mod);
    if (rv != 0)
    {
        LOG(LOG_LEVEL_ERROR, "mod_connect: macos_capture_start failed");
        macos_capture_deinit(mod);
        return rv;
    }

    /* Send initial screen */
    if (mod->server_reset)
    {
        mod->server_reset(mod, mod->width, mod->height, mod->bpp);
    }

    return 0;
}

/**
 * Handle events (mouse, keyboard)
 */
int EXPORT_CC
mod_event(tintptr handle, int msg, tbus param1, tbus param2, tbus param3, tbus param4)
{
    struct mod_macos* mod = (struct mod_macos*)handle;

    if (mod == NULL)
    {
        return 1;
    }

    switch (msg)
    {
        case 15: /* WM_MOUSEMOVE */
        case 16: /* WM_LBUTTONDOWN */
        case 17: /* WM_LBUTTONUP */
        case 18: /* WM_RBUTTONDOWN */
        case 19: /* WM_RBUTTONUP */
        case 20: /* WM_MBUTTONDOWN */
        case 21: /* WM_MBUTTONUP */
            return macos_input_mouse_event(mod, param1, param2, param3);

        case 256: /* WM_KEYDOWN */
        case 257: /* WM_KEYUP */
            return macos_input_keyboard_event(mod, param1, param2);

        default:
            break;
    }

    return 0;
}

/**
 * Check for screen updates
 */
int EXPORT_CC
mod_signal(tintptr handle)
{
    struct mod_macos* mod = (struct mod_macos*)handle;

    if (mod == NULL)
    {
        return 1;
    }

    /* Check if we have damage and need to send a frame */
    if (mod->has_damage)
    {
        return macos_capture_get_frame(mod);
    }

    return 0;
}

/**
 * End module session
 */
int EXPORT_CC
mod_end(tintptr handle)
{
    struct mod_macos* mod = (struct mod_macos*)handle;

    LOG(LOG_LEVEL_INFO, "mod_end");

    if (mod == NULL)
    {
        return 0;
    }

    macos_capture_deinit(mod);

    return 0;
}

/**
 * Set module parameter
 */
int EXPORT_CC
mod_set_param(tintptr handle, char* name, char* value)
{
    struct mod_macos* mod = (struct mod_macos*)handle;

    if (mod == NULL)
    {
        return 1;
    }

    LOG(LOG_LEVEL_DEBUG, "mod_set_param: %s = %s", name, value);

    return 0;
}

/**
 * Get wait objects for select/poll
 */
int EXPORT_CC
mod_get_wait_objs(tintptr handle, tbus* read_objs, int* rcount,
                  tbus* write_objs, int* wcount, int* timeout)
{
    struct mod_macos* mod = (struct mod_macos*)handle;

    if (mod == NULL)
    {
        return 1;
    }

    /* Set timeout for frame capture (30 FPS = ~33ms) */
    *timeout = 33;

    return 0;
}

/**
 * Check wait objects
 */
int EXPORT_CC
mod_check_wait_objs(tintptr handle)
{
    struct mod_macos* mod = (struct mod_macos*)handle;

    if (mod == NULL)
    {
        return 1;
    }

    /* Trigger frame check */
    return mod_signal(handle);
}

/**
 * Frame acknowledgment from client
 */
int EXPORT_CC
mod_frame_ack(tintptr handle, int flags, int frame_id)
{
    return 0;
}

/**
 * Suppress output (not implemented)
 */
int EXPORT_CC
mod_suppress_output(tintptr handle, int suppress,
                    int left, int top, int right, int bottom)
{
    return 0;
}

/**
 * Server version message
 */
int EXPORT_CC
mod_server_version_message(tintptr handle)
{
    return 0;
}

/**
 * Handle monitor resize
 */
int EXPORT_CC
mod_server_monitor_resize(tintptr handle, int width, int height)
{
    struct mod_macos* mod = (struct mod_macos*)handle;

    if (mod == NULL)
    {
        return 1;
    }

    LOG(LOG_LEVEL_INFO, "mod_server_monitor_resize: %dx%d", width, height);

    /* Restart capture with new dimensions */
    macos_capture_stop(mod);

    mod->width = width;
    mod->height = height;

    /* Reallocate frame buffer */
    if (mod->frame_buffer)
    {
        g_free(mod->frame_buffer);
    }

    mod->frame_buffer_size = width * height * 4;
    mod->frame_buffer = g_malloc(mod->frame_buffer_size, 0);
    if (mod->frame_buffer == NULL)
    {
        return 1;
    }

    return macos_capture_start(mod);
}
