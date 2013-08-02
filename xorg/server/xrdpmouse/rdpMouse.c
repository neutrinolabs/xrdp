/*
Copyright 2013 Jay Sorg

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

xrdp mouse module

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* this should be before all X11 .h files */
#include <xorg-server.h>

/* all driver need this */
#include <xf86.h>
#include <xf86_OSproc.h>

#include "xf86Xinput.h"

#include <mipointer.h>
#include <fb.h>
#include <micmap.h>
#include <mi.h>
#include <exevents.h>
#include <xserver-properties.h>

#include "rdp.h"
#include "rdpInput.h"
#include "rdpDraw.h"

/******************************************************************************/
#define LOG_LEVEL 1
#define LLOGLN(_level, _args) \
    do { if (_level < LOG_LEVEL) { ErrorF _args ; ErrorF("\n"); } } while (0)

#define XRDP_DRIVER_NAME "XRDPMOUSE"
#define XRDP_NAME "XRDPMOUSE"
#define XRDP_VERSION 1000

#define PACKAGE_VERSION_MAJOR 1
#define PACKAGE_VERSION_MINOR 0
#define PACKAGE_VERSION_PATCHLEVEL 0

/******************************************************************************/
static void
rdpmouseDeviceInit(void)
{
    LLOGLN(0, ("rdpmouseDeviceInit:"));
}

/******************************************************************************/
static void
rdpmouseDeviceOn(DeviceIntPtr pDev)
{
    LLOGLN(0, ("rdpmouseDeviceOn:"));
}

/******************************************************************************/
static void
rdpmouseDeviceOff(void)
{
    LLOGLN(0, ("rdpmouseDeviceOff:"));
}

/******************************************************************************/
static void
rdpmouseCtrl(DeviceIntPtr pDevice, PtrCtrl *pCtrl)
{
    LLOGLN(0, ("rdpmouseCtrl:"));
}

/******************************************************************************/
static int
l_bound_by(int val, int low, int high)
{
    val = RDPCLAMP(val, low, high);
    return val;
}

/******************************************************************************/
static void
rdpEnqueueMotion(DeviceIntPtr device, int x, int y)
{
    int valuators[2];

    valuators[0] = x;
    valuators[1] = y;
    xf86PostMotionEvent(device, TRUE, 0, 2, valuators);
}

/******************************************************************************/
static void
rdpEnqueueButton(DeviceIntPtr device, int type, int buttons)
{
    xf86PostButtonEvent(device, FALSE, buttons, type, 0, 0);
}

/******************************************************************************/
static void
PtrAddEvent(rdpPointer *pointer)
{
    int i;
    int type;
    int buttons;

    rdpEnqueueMotion(pointer->device, pointer->cursor_x, pointer->cursor_y);

    for (i = 0; i < 5; i++)
    {
        if ((pointer->button_mask ^ pointer->old_button_mask) & (1 << i))
        {
            if (pointer->button_mask & (1 << i))
            {
                type = ButtonPress;
                buttons = i + 1;
                rdpEnqueueButton(pointer->device, type, buttons);
            }
            else
            {
                type = ButtonRelease;
                buttons = i + 1;
                rdpEnqueueButton(pointer->device, type, buttons);
            }
        }
    }

    pointer->old_button_mask = pointer->button_mask;
}

/******************************************************************************/
static int
rdpInputMouse(rdpPtr dev, int msg,
              long param1, long param2,
              long param3, long param4)
{
    rdpPointer *pointer;

    LLOGLN(0, ("rdpInputMouse:"));
    pointer = &(dev->pointer);
    switch (msg)
    {
        case 100:
            /* without the minus 2, strange things happen when dragging
               past the width or height */
            pointer->cursor_x = l_bound_by(param1, 0, dev->width - 2);
            pointer->cursor_y = l_bound_by(param2, 0, dev->height - 2);
            PtrAddEvent(pointer);
            break;
        case 101:
            pointer->button_mask = pointer->button_mask & (~1);
            PtrAddEvent(pointer);
            break;
        case 102:
            pointer->button_mask = pointer->button_mask | 1;
            PtrAddEvent(pointer);
            break;
        case 103:
            pointer->button_mask = pointer->button_mask & (~4);
            PtrAddEvent(pointer);
            break;
        case 104:
            pointer->button_mask = pointer->button_mask | 4;
            PtrAddEvent(pointer);
            break;
        case 105:
            pointer->button_mask = pointer->button_mask & (~2);
            PtrAddEvent(pointer);
            break;
        case 106:
            pointer->button_mask = pointer->button_mask | 2;
            PtrAddEvent(pointer);
            break;
        case 107:
            pointer->button_mask = pointer->button_mask & (~8);
            PtrAddEvent(pointer);
            break;
        case 108:
            pointer->button_mask = pointer->button_mask | 8;
            PtrAddEvent(pointer);
            break;
        case 109:
            pointer->button_mask = pointer->button_mask & (~16);
            PtrAddEvent(pointer);
            break;
        case 110:
            pointer->button_mask = pointer->button_mask | 16;
            PtrAddEvent(pointer);
            break;
    }
    return 0;
}

/******************************************************************************/
static int
rdpmouseControl(DeviceIntPtr device, int what)
{
    BYTE map[6];
    DevicePtr pDev;
    Atom btn_labels[6];
    Atom axes_labels[2];
    rdpPtr dev;

    LLOGLN(0, ("rdpmouseControl: what %d", what));
    pDev = (DevicePtr)device;

    switch (what)
    {
        case DEVICE_INIT:
            rdpmouseDeviceInit();
            map[0] = 0;
            map[1] = 1;
            map[2] = 2;
            map[3] = 3;
            map[4] = 4;
            map[5] = 5;

            btn_labels[0] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_LEFT);
            btn_labels[1] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_MIDDLE);
            btn_labels[2] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_RIGHT);
            btn_labels[3] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_WHEEL_UP);
            btn_labels[4] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_WHEEL_DOWN);

            axes_labels[0] = XIGetKnownProperty(AXIS_LABEL_PROP_REL_X);
            axes_labels[1] = XIGetKnownProperty(AXIS_LABEL_PROP_REL_Y);

            InitPointerDeviceStruct(pDev, map, 5, btn_labels, rdpmouseCtrl,
                                    GetMotionHistorySize(), 2, axes_labels);
            dev = rdpGetDevFromScreen(NULL);
            dev->pointer.device = device;
            rdpRegisterInputCallback(1, rdpInputMouse);
            break;
        case DEVICE_ON:
            pDev->on = 1;
            rdpmouseDeviceOn(device);
            break;
        case DEVICE_OFF:
            pDev->on = 0;
            rdpmouseDeviceOff();
            break;
        case DEVICE_CLOSE:

            if (pDev->on)
            {
                rdpmouseDeviceOff();
            }

            break;
    }

    return Success;
}

#if XORG_VERSION_CURRENT < (((1) * 10000000) + ((9) * 100000) + ((0) * 1000) + 1)

/* debian 6
   ubuntu 10.04 */

/******************************************************************************/
static InputInfoPtr
rdpmousePreInit(InputDriverPtr drv, IDevPtr dev, int flags)
{
    InputInfoPtr info;

    LLOGLN(0, ("rdpmousePreInit: drv %p dev %p, flags 0x%x",
           drv, dev, flags));
    info = xf86AllocateInput(drv, 0);
    info->name = dev->identifier;
    info->device_control = rdpmouseControl;
    info->flags = XI86_CONFIGURED | XI86_ALWAYS_CORE | XI86_SEND_DRAG_EVENTS |
                  XI86_CORE_POINTER | XI86_POINTER_CAPABLE;
    info->type_name = "Mouse";
    info->fd = -1;
    info->conf_idev = dev;

    return info;
}

#else

/* debian 7
   ubuntu 12.04 */

/******************************************************************************/
static int
rdpmousePreInit(InputDriverPtr drv, InputInfoPtr info, int flags)
{
    LLOGLN(0, ("rdpmousePreInit: drv %p info %p, flags 0x%x",
           drv, info, flags));
    info->device_control = rdpmouseControl;
    info->type_name = "Mouse";
    return 0;
}

#endif

/******************************************************************************/
static void
rdpmouseUnInit(InputDriverPtr drv, InputInfoPtr info, int flags)
{
    LLOGLN(0, ("rdpmouseUnInit: drv %p info %p, flags 0x%x",
           drv, info, flags));
}

/******************************************************************************/
static InputDriverRec rdpmouse =
{
    PACKAGE_VERSION_MAJOR,    /* version   */
    XRDP_NAME,                /* name      */
    NULL,                     /* identify  */
    rdpmousePreInit,          /* preinit   */
    rdpmouseUnInit,           /* uninit    */
    NULL,                     /* module    */
    0                         /* ref count */
};

/******************************************************************************/
static void
rdpmouseUnplug(pointer p)
{
    LLOGLN(0, ("rdpmouseUnplug:"));
}

/******************************************************************************/
static pointer
rdpmousePlug(pointer module, pointer options, int *errmaj, int *errmin)
{
    LLOGLN(0, ("rdpmousePlug:"));
    xf86AddInputDriver(&rdpmouse, module, 0);
    return module;
}

/******************************************************************************/
static XF86ModuleVersionInfo rdpmouseVersionRec =
{
    XRDP_DRIVER_NAME,
    MODULEVENDORSTRING,
    MODINFOSTRING1,
    MODINFOSTRING2,
    XORG_VERSION_CURRENT,
    PACKAGE_VERSION_MAJOR,
    PACKAGE_VERSION_MINOR,
    PACKAGE_VERSION_PATCHLEVEL,
    ABI_CLASS_XINPUT,
    ABI_XINPUT_VERSION,
    MOD_CLASS_XINPUT,
    { 0, 0, 0, 0 }
};

/******************************************************************************/
XF86ModuleData xrdpmouseModuleData =
{
    &rdpmouseVersionRec,
    rdpmousePlug,
    rdpmouseUnplug
};
