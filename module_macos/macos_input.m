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
 * @file macos_input.m
 * @brief Mouse and keyboard input handling for macOS
 */

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <Carbon/Carbon.h>

#include "macos_capture.h"

/* RDP mouse button flags */
#define MOUSE_FLAG_MOVE     0x0800
#define MOUSE_FLAG_BUTTON1  0x1000  /* left button */
#define MOUSE_FLAG_BUTTON2  0x2000  /* right button */
#define MOUSE_FLAG_BUTTON3  0x4000  /* middle button */
#define MOUSE_FLAG_BUTTON4  0x0280  /* wheel up */
#define MOUSE_FLAG_BUTTON5  0x0380  /* wheel down */
#define MOUSE_FLAG_DOWN     0x8000

/**
 * Handle mouse events from RDP client
 */
int
macos_input_mouse_event(struct mod_macos* mod, int x, int y, int flags)
{
    CGEventRef event = NULL;
    CGPoint point;
    CGMouseButton button;
    int32_t scroll_delta;

    /* Update stored mouse position */
    mod->mouse_x = x;
    mod->mouse_y = y;

    /* Convert to screen coordinates */
    point = CGPointMake((CGFloat)x, (CGFloat)y);

    /* Handle mouse movement */
    if (flags & MOUSE_FLAG_MOVE)
    {
        event = CGEventCreateMouseEvent(NULL, kCGEventMouseMoved, point, kCGMouseButtonLeft);
        if (event)
        {
            CGEventPost(kCGHIDEventTap, event);
            CFRelease(event);
        }
    }

    /* Handle left button */
    if (flags & MOUSE_FLAG_BUTTON1)
    {
        button = kCGMouseButtonLeft;

        if (flags & MOUSE_FLAG_DOWN)
        {
            /* Button down */
            event = CGEventCreateMouseEvent(NULL, kCGEventLeftMouseDown, point, button);
            mod->mouse_buttons |= 0x01;
        }
        else
        {
            /* Button up */
            event = CGEventCreateMouseEvent(NULL, kCGEventLeftMouseUp, point, button);
            mod->mouse_buttons &= ~0x01;
        }

        if (event)
        {
            CGEventPost(kCGHIDEventTap, event);
            CFRelease(event);
        }
    }

    /* Handle right button */
    if (flags & MOUSE_FLAG_BUTTON2)
    {
        button = kCGMouseButtonRight;

        if (flags & MOUSE_FLAG_DOWN)
        {
            /* Button down */
            event = CGEventCreateMouseEvent(NULL, kCGEventRightMouseDown, point, button);
            mod->mouse_buttons |= 0x02;
        }
        else
        {
            /* Button up */
            event = CGEventCreateMouseEvent(NULL, kCGEventRightMouseUp, point, button);
            mod->mouse_buttons &= ~0x02;
        }

        if (event)
        {
            CGEventPost(kCGHIDEventTap, event);
            CFRelease(event);
        }
    }

    /* Handle middle button */
    if (flags & MOUSE_FLAG_BUTTON3)
    {
        button = kCGMouseButtonCenter;

        if (flags & MOUSE_FLAG_DOWN)
        {
            /* Button down */
            event = CGEventCreateMouseEvent(NULL, kCGEventOtherMouseDown, point, button);
            mod->mouse_buttons |= 0x04;
        }
        else
        {
            /* Button up */
            event = CGEventCreateMouseEvent(NULL, kCGEventOtherMouseUp, point, button);
            mod->mouse_buttons &= ~0x04;
        }

        if (event)
        {
            CGEventPost(kCGHIDEventTap, event);
            CFRelease(event);
        }
    }

    /* Handle mouse wheel */
    if (flags & (MOUSE_FLAG_BUTTON4 | MOUSE_FLAG_BUTTON5))
    {
        scroll_delta = (flags & MOUSE_FLAG_BUTTON4) ? 1 : -1;

        event = CGEventCreateScrollWheelEvent(NULL, kCGScrollEventUnitLine, 1, scroll_delta);
        if (event)
        {
            CGEventPost(kCGHIDEventTap, event);
            CFRelease(event);
        }
    }

    return 0;
}

/**
 * Handle keyboard events from RDP client
 */
int
macos_input_keyboard_event(struct mod_macos* mod, int flags, int key)
{
    CGEventRef event = NULL;
    CGKeyCode keycode;
    bool keyDown;

    /* Determine key state */
    keyDown = !(flags & 0x8000); /* Key up flag is 0x8000 */

    /* Map RDP scancode to macOS keycode */
    /* This is a simplified mapping - full implementation would need complete scancode table */
    keycode = (CGKeyCode)key;

    /* Create keyboard event */
    event = CGEventCreateKeyboardEvent(NULL, keycode, keyDown);
    if (event)
    {
        /* Post event */
        CGEventPost(kCGHIDEventTap, event);
        CFRelease(event);
    }

    return 0;
}
