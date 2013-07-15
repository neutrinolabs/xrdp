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
rdpMouseDeviceInit(void)
{
    LLOGLN(0, ("rdpMouseDeviceInit:"));
}

/******************************************************************************/
static void
rdpMouseDeviceOn(DeviceIntPtr pDev)
{
    LLOGLN(0, ("rdpMouseDeviceOn:"));
}

/******************************************************************************/
static void
rdpMouseDeviceOff(void)
{
    LLOGLN(0, ("rdpMouseDeviceOff:"));
}

/******************************************************************************/
static void
rdpMouseCtrl(DeviceIntPtr pDevice, PtrCtrl *pCtrl)
{
    LLOGLN(0, ("rdpMouseCtrl:"));
}

/******************************************************************************/
static int
rdpMouseControl(DeviceIntPtr device, int what)
{
    BYTE map[6];
    DevicePtr pDev;
    Atom btn_labels[6];
    Atom axes_labels[2];

    LLOGLN(0, ("rdpMouseControl: what %d", what));
    pDev = (DevicePtr)device;

    switch (what)
    {
        case DEVICE_INIT:
            rdpMouseDeviceInit();
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

            InitPointerDeviceStruct(pDev, map, 5, btn_labels, rdpMouseCtrl,
                                    GetMotionHistorySize(), 2, axes_labels);

            break;
        case DEVICE_ON:
            pDev->on = 1;
            rdpMouseDeviceOn(device);
            break;
        case DEVICE_OFF:
            pDev->on = 0;
            rdpMouseDeviceOff();
            break;
        case DEVICE_CLOSE:

            if (pDev->on)
            {
                rdpMouseDeviceOff();
            }

            break;
    }

    return Success;
}

/******************************************************************************/
static void
rdpMouseReadInput(struct _InputInfoRec *local)
{
    LLOGLN(0, ("rdpMouseReadInput:"));
}

/******************************************************************************/
static int
rdpMouseControlProc(struct _InputInfoRec *local, xDeviceCtl *control)
{
    LLOGLN(0, ("rdpMouseControlProc:"));
    return 0;
}

/******************************************************************************/
static int
rdpMouseSwitchMode(ClientPtr client, DeviceIntPtr dev, int mode)
{
    LLOGLN(0, ("rdpMouseSwitchMode:"));
    return 0;
}

/******************************************************************************/
static int
rdpMouseSetDeviceValuators(struct _InputInfoRec *local,
                           int *valuators, int first_valuator,
                           int num_valuators)
{
    LLOGLN(0, ("rdpMouseSetDeviceValuators:"));
    return 0;
}

/******************************************************************************/
static int
rdpmousePreInit(InputDriverPtr drv, InputInfoPtr info, int flags)
{
    LLOGLN(0, ("rdpmousePreInit: drv %p info %p, flags 0x%x",
           drv, info, flags));
    info->device_control = rdpMouseControl;
    info->read_input = rdpMouseReadInput;
    info->control_proc = rdpMouseControlProc;
    info->switch_mode = rdpMouseSwitchMode;
    info->set_device_valuators = rdpMouseSetDeviceValuators;
    info->type_name = "Mouse";
    return 0;
}

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
