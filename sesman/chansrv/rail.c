/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2012
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

/*
   window manager info
   http://www.freedesktop.org/wiki/Specifications/wm-spec

   rail
   [MS-RDPERP]: Remote Desktop Protocol:
   Remote Programs Virtual Channel Extension
   http://msdn.microsoft.com/en-us/library/cc242568(v=prot.20).aspx
*/

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include "chansrv.h"
#include "rail.h"
#include "xcommon.h"
#include "log.h"
#include "os_calls.h"
#include "thread_calls.h"
#include "list.h"

extern int g_rail_chan_id;      /* in chansrv.c */
extern int g_display_num;       /* in chansrv.c */
extern char *g_exec_name;       /* in chansrv.c */
extern tbus g_exec_event;       /* in chansrv.c */
extern tbus g_exec_mutex;       /* in chansrv.c */
extern tbus g_exec_sem;         /* in chansrv.c */

extern Display *g_display;           /* in xcommon.c */
extern Screen *g_screen;             /* in xcommon.c */
extern Window g_root_window;         /* in xcommon.c */
extern Atom g_wm_delete_window_atom; /* in xcommon.c */
extern Atom g_wm_protocols_atom;     /* in xcommon.c */
extern Atom g_utf8_string;           /* in xcommon.c */
extern Atom g_net_wm_name;           /* in xcommon.c */
extern Atom g_wm_state;              /* in xcommon.c */

static Atom g_rwd_atom = 0;

int g_rail_up = 0;

/* for rail_is_another_wm_running */
static int g_rail_running = 1;
/* list of valid rail windows */
static struct list* g_window_list = 0;

/* used in valid field of struct rail_window_data */
#define RWD_X       (1 << 0)
#define RWD_Y       (1 << 1)
#define RWD_WIDTH   (1 << 2)
#define RWD_HEIGHT  (1 << 3)
#define RWD_TITLE   (1 << 4)

struct rail_window_data
{
    int valid; /* bits for which fields are valid */
    int x;
    int y;
    int width;
    int height;
    char title[64]; /* first 64 bytes of title for compare */
};

/* Indicates a Client Execute PDU from client to server. */
#define TS_RAIL_ORDER_EXEC 0x0001
/* Indicates a Client Activate PDU from client to server. */
#define TS_RAIL_ORDER_ACTIVATE 0x0002
/* Indicates a Client System Parameters Update PDU from client
   to server or a Server System Parameters Update PDU
   from server to client. */
#define TS_RAIL_ORDER_SYSPARAM 0x0003
/* Indicates a Client System Command PDU from client to server. */
#define TS_RAIL_ORDER_SYSCOMMAND 0x0004
/* Indicates a bi-directional Handshake PDU. */
#define TS_RAIL_ORDER_HANDSHAKE 0x0005
/* Indicates a Client Notify Event PDU from client to server. */
#define TS_RAIL_ORDER_NOTIFY_EVENT 0x0006
/* Indicates a Client Window Move PDU from client to server. */
#define TS_RAIL_ORDER_WINDOWMOVE 0x0008
/* Indicates a Server Move/Size Start PDU and a Server Move/Size
   End PDU from server to client. */
#define TS_RAIL_ORDER_LOCALMOVESIZE 0x0009
/* Indicates a Server Min Max Info PDU from server to client. */
#define TS_RAIL_ORDER_MINMAXINFO 0x000a
/* Indicates a Client Information PDU from client to server. */
#define TS_RAIL_ORDER_CLIENTSTATUS 0x000b
/* Indicates a Client System Menu PDU from client to server. */
#define TS_RAIL_ORDER_SYSMENU 0x000c
/* Indicates a Server Language Bar Information PDU from server to
   client, or a Client Language Bar Information PDU from client to server. */
#define TS_RAIL_ORDER_LANGBARINFO 0x000d
/* Indicates a Server Execute Result PDU from server to client. */
#define TS_RAIL_ORDER_EXEC_RESULT 0x0080
/* Indicates a Client Get Application ID PDU from client to server. */
#define TS_RAIL_ORDER_GET_APPID_REQ 0x000E
/* Indicates a Server Get Application ID Response PDU from
   server to client. */
#define TS_RAIL_ORDER_GET_APPID_RESP 0x000F

/* Resize the window. */
#define SC_SIZE 0xF000
/* Move the window. */
#define SC_MOVE 0xF010
/* Minimize the window. */
#define SC_MINIMIZE 0xF020
/* Maximize the window. */
#define SC_MAXIMIZE 0xF030
/* Close the window. */
#define SC_CLOSE 0xF060
/* The ALT + SPACE key combination was pressed; display
   the window's system menu. */
#define SC_KEYMENU 0xF100
/* Restore the window to its original shape and size. */
#define SC_RESTORE 0xF120
/* Perform the default action of the window's system menu. */
#define SC_DEFAULT 0xF160

/* for tooltips */
#define RAIL_STYLE_TOOLTIP (0x80000000)
#define RAIL_EXT_STYLE_TOOLTIP (0x00000080 | 0x00000008)

/* for normal desktop windows */
#define RAIL_STYLE_NORMAL (0x00C00000 | 0x00080000 | 0x00040000 | 0x00010000 | 0x00020000)
#define RAIL_EXT_STYLE_NORMAL (0x00040000)

/* for dialogs */
#define RAIL_STYLE_DIALOG (0x80000000)
#define RAIL_EXT_STYLE_DIALOG (0x00040000)

static int APP_CC rail_win_get_state(Window win);
static int APP_CC rail_create_window(Window window_id, Window owner_id);
static int APP_CC rail_win_set_state(Window win, unsigned long state);
static int APP_CC rail_show_window(Window window_id, int show_state);
static int APP_CC rail_win_send_text(Window win);

/*****************************************************************************/
static struct rail_window_data* APP_CC
rail_get_window_data(Window window)
{
    int bytes;
    Atom actual_type_return;
    int actual_format_return;
    unsigned long nitems_return;
    unsigned long bytes_after_return;
    unsigned char* prop_return;
    struct rail_window_data* rv;

    LOG(10, ("chansrv::rail_get_window_data:"));
    rv = 0;
    actual_type_return = 0;
    actual_format_return = 0;
    nitems_return = 0;
    prop_return = 0;
    bytes = sizeof(struct rail_window_data);
    XGetWindowProperty(g_display, window, g_rwd_atom, 0, bytes, 0,
                       XA_STRING, &actual_type_return,
                       &actual_format_return, &nitems_return,
                       &bytes_after_return, &prop_return);
    if (prop_return == 0)
    {
        return 0;
    }
    if (nitems_return == bytes)
    {
        rv = (struct rail_window_data*)prop_return;
    }
    return rv;
}

/*****************************************************************************/
static int APP_CC
rail_set_window_data(Window window, struct rail_window_data* rwd)
{
    int bytes;

    bytes = sizeof(struct rail_window_data);
    XChangeProperty(g_display, window, g_rwd_atom, XA_STRING, 8,
                    PropModeReplace, (unsigned char*)rwd, bytes);
    return 0;
}

/*****************************************************************************/
/* get the rail window data, if not exist, try to create it and return */
static struct rail_window_data* APP_CC
rail_get_window_data_safe(Window window)
{
    struct rail_window_data* rv;
    
    rv = rail_get_window_data(window);
    if (rv != 0)
    {
        return rv;
    }
    rv = g_malloc(sizeof(struct rail_window_data), 1);
    rail_set_window_data(window, rv);
    g_free(rv);
    return rail_get_window_data(window);
}

/******************************************************************************/
static int APP_CC
is_window_valid_child_of_root(unsigned int window_id)
{
    int found;
    unsigned int i;
    unsigned int nchild;
    Window r;
    Window p;
    Window *children;

    found = 0;
    XQueryTree(g_display, g_root_window, &r, &p, &children, &nchild);

    for (i = 0; i < nchild; i++)
    {
        if (window_id == children[i])
        {
            found = 1;
            break;
        }
    }

    XFree(children);
    return found;
}

/*****************************************************************************/
static int APP_CC
rail_send_init(void)
{
    struct stream *s;
    int bytes;
    char *size_ptr;

    LOG(10, ("chansrv::rail_send_init:"));
    make_stream(s);
    init_stream(s, 8182);
    out_uint16_le(s, TS_RAIL_ORDER_HANDSHAKE);
    size_ptr = s->p;
    out_uint16_le(s, 0);        /* size, set later */
    out_uint32_le(s, 1);        /* build number */
    s_mark_end(s);
    bytes = (int)((s->end - s->data) - 4);
    size_ptr[0] = bytes;
    size_ptr[1] = bytes >> 8;
    bytes = (int)(s->end - s->data);
    send_channel_data(g_rail_chan_id, s->data, bytes);
    free_stream(s);
    return 0;
}

/******************************************************************************/
static int DEFAULT_CC
anotherWMRunning(Display *display, XErrorEvent *xe)
{
    g_rail_running = 0;
    return -1;
}

/******************************************************************************/
static int APP_CC
rail_is_another_wm_running(void)
{
    XErrorHandler old;

    g_rail_running = 1;
    old = XSetErrorHandler((XErrorHandler)anotherWMRunning);
    XSelectInput(g_display, g_root_window,
                 PropertyChangeMask | StructureNotifyMask |
                 SubstructureRedirectMask | ButtonPressMask |
                 SubstructureNotifyMask | FocusChangeMask |
                 EnterWindowMask | LeaveWindowMask);
    XSync(g_display, 0);
    XSetErrorHandler((XErrorHandler)old);
    g_rail_up = g_rail_running;

    if (!g_rail_up)
    {
        return 1;
    }

    return 0;
}

/*****************************************************************************/
int APP_CC
rail_init(void)
{
    LOG(10, ("chansrv::rail_init:"));
    xcommon_init();

    if (rail_is_another_wm_running())
    {
        log_message(LOG_LEVEL_ERROR, "rail_init: another window manager "
                    "is running");
    }
    list_delete(g_window_list);
    g_window_list = list_create();
    rail_send_init();
    g_rail_up = 1;
    g_rwd_atom = XInternAtom(g_display, "XRDP_RAIL_WINDOW_DATA", 0);
    return 0;
}

/*****************************************************************************/
int APP_CC
rail_deinit(void)
{
    if (g_rail_up)
    {
        list_delete(g_window_list);
        g_window_list = 0;
        /* no longer window manager */
        XSelectInput(g_display, g_root_window, 0);
        g_rail_up = 0;
    }

    return 0;
}

/*****************************************************************************/
static char *APP_CC
read_uni(struct stream *s, int num_chars)
{
    twchar *rchrs;
    char *rv;
    int index;
    int lchars;

    rchrs = 0;
    rv = 0;

    if (num_chars > 0)
    {
        rchrs = (twchar *)g_malloc((num_chars + 1) * sizeof(twchar), 0);

        for (index = 0; index < num_chars; index++)
        {
            in_uint16_le(s, rchrs[index]);
        }

        rchrs[num_chars] = 0;
        lchars = g_wcstombs(0, rchrs, 0);

        if (lchars > 0)
        {
            rv = (char *)g_malloc((lchars + 1) * 4, 0);
            g_wcstombs(rv, rchrs, lchars);
            rv[lchars] = 0;
        }
    }

    g_free(rchrs);
    return rv;
}

/*****************************************************************************/
static int APP_CC
rail_process_exec(struct stream *s, int size)
{
    int flags;
    int ExeOrFileLength;
    int WorkingDirLength;
    int ArgumentsLen;
    char *ExeOrFile;
    char *WorkingDir;
    char *Arguments;

    LOG(0, ("chansrv::rail_process_exec:"));
    in_uint16_le(s, flags);
    in_uint16_le(s, ExeOrFileLength);
    in_uint16_le(s, WorkingDirLength);
    in_uint16_le(s, ArgumentsLen);
    ExeOrFile = read_uni(s, ExeOrFileLength);
    WorkingDir = read_uni(s, WorkingDirLength);
    Arguments = read_uni(s, ArgumentsLen);
    LOG(10, ("  flags 0x%8.8x ExeOrFileLength %d WorkingDirLength %d "
             "ArgumentsLen %d ExeOrFile [%s] WorkingDir [%s] "
             "Arguments [%s]", flags, ExeOrFileLength, WorkingDirLength,
             ArgumentsLen, ExeOrFile, WorkingDir, Arguments));

    if (g_strlen(ExeOrFile) > 0)
    {
        LOG(10, ("rail_process_exec: pre"));
        /* ask main thread to fork */
        tc_mutex_lock(g_exec_mutex);
        g_exec_name = ExeOrFile;
        g_set_wait_obj(g_exec_event);
        tc_sem_dec(g_exec_sem);
        tc_mutex_unlock(g_exec_mutex);
        LOG(10, ("rail_process_exec: post"));
    }

    g_free(ExeOrFile);
    g_free(WorkingDir);
    g_free(Arguments);
    return 0;
}

/*****************************************************************************/
static void APP_CC
rail_simulate_mouse_click(int button)
{
    /*
     * The below code can be referenced from:
     * http://www.linuxquestions.org/questions/programming-9/simulating-a-mouse-click-594576/#post2936738
     */
    XEvent event;
    g_memset(&event, 0x00, sizeof(event));
    
    event.type = ButtonPress;
    event.xbutton.button = button;
    event.xbutton.same_screen = True;
    
    XQueryPointer(g_display, g_root_window, &event.xbutton.root,
                  &event.xbutton.window, &event.xbutton.x_root,
                  &event.xbutton.y_root, &event.xbutton.x,
                  &event.xbutton.y, &event.xbutton.state);
    
    event.xbutton.subwindow = event.xbutton.window;
    
    while(event.xbutton.subwindow)
    {
        event.xbutton.window = event.xbutton.subwindow;
        
        XQueryPointer(g_display, event.xbutton.window, &event.xbutton.root,
                      &event.xbutton.subwindow, &event.xbutton.x_root,
                      &event.xbutton.y_root, &event.xbutton.x,
                      &event.xbutton.y, &event.xbutton.state);
    }
    
    if(XSendEvent(g_display, PointerWindow, True, 0xfff, &event) == 0)
    {
        LOG(0, ("  error sending mouse event"));
    }
    
    XFlush(g_display);
    
    g_sleep(100);
    
    event.type = ButtonRelease;
    event.xbutton.state = 0x100;
    
    if(XSendEvent(g_display, PointerWindow, True, 0xfff, &event) == 0)
    {
        LOG(0, ("  error sending mouse event"));
    }
    
    XFlush(g_display);
}

/******************************************************************************/
static int APP_CC
rail_win_popdown(int window_id)
{
    int rv = 0;
    unsigned int i;
    unsigned int nchild;
    Window r;
    Window p;
    Window* children;
    
    /*
     * Check the tree of current existing X windows and dismiss
     * the managed rail popups by simulating a mouse click, so
     * that the requested window can be closed properly.
     */
    
    XQueryTree(g_display, g_root_window, &r, &p, &children, &nchild);
    for (i = 0; i < nchild; i++)
    {
        XWindowAttributes window_attributes;
        XGetWindowAttributes(g_display, children[i], &window_attributes);
        if (window_attributes.override_redirect &&
            window_attributes.map_state == IsViewable &&
            list_index_of(g_window_list, children[i]) >= 0) {
            LOG(0, ("  dismiss pop up 0x%8.8x", children[i]));
            rail_simulate_mouse_click(Button1);
            rv = 1;
            break;
        }
    }
    
    XFree(children);
    return rv;
}

/******************************************************************************/
static int APP_CC
rail_close_window(int window_id)
{
    XEvent ce;
    
    LOG(0, ("chansrv::rail_close_window:"));
    
    if (rail_win_popdown(window_id))
    {
        return 0;
    }
    
    /* don't receive UnmapNotify for closing window */
    XSelectInput(g_display, window_id, PropertyChangeMask);
    
    g_memset(&ce, 0, sizeof(ce));
    ce.xclient.type = ClientMessage;
    ce.xclient.message_type = g_wm_protocols_atom;
    ce.xclient.display = g_display;
    ce.xclient.window = window_id;
    ce.xclient.format = 32;
    ce.xclient.data.l[0] = g_wm_delete_window_atom;
    ce.xclient.data.l[1] = CurrentTime;
    XSendEvent(g_display, window_id, False, NoEventMask, &ce);

    return 0;
}

/*****************************************************************************/
static int APP_CC
rail_process_activate(struct stream *s, int size)
{
    int window_id;
    int enabled;
    XWindowAttributes window_attributes;
    Window transient_for = 0;

    LOG(10, ("chansrv::rail_process_activate:"));
    in_uint32_le(s, window_id);
    in_uint8(s, enabled);
    LOG(10, ("  window_id 0x%8.8x enabled %d", window_id, enabled));

    XGetWindowAttributes(g_display, window_id, &window_attributes);
    
    if (enabled)
    {
        if (window_attributes.map_state != IsViewable)
        {
            /* In case that window is unmapped upon minimization and not yet mapped*/
            XMapWindow(g_display, window_id);
        }
        XGetTransientForHint(g_display, window_id, &transient_for);
        if (transient_for > 0)
        {
            // Owner window should be raised up as well
            XRaiseWindow(g_display, transient_for);
        }
        LOG(10, ("chansrv::rail_process_activate: calling XRaiseWindow 0x%8.8x", window_id));
        XRaiseWindow(g_display, window_id);
        LOG(10, ("chansrv::rail_process_activate: calling XSetInputFocus 0x%8.8x", window_id));
        XSetInputFocus(g_display, window_id, RevertToParent, CurrentTime);
    } else {
        XWindowAttributes window_attributes;
        XGetWindowAttributes(g_display, window_id, &window_attributes);

        LOG(10, ("  window attributes: override_redirect %d",
                 window_attributes.override_redirect));
        if (window_attributes.override_redirect) {
            LOG(10, ("  dismiss popup window 0x%8.8x", window_id));
            XUnmapWindow(g_display, window_id);
        }
    }
    return 0;
}

/*****************************************************************************/
static int APP_CC
rail_restore_windows(void)
{
    unsigned int i;
    unsigned int nchild;
    Window r;
    Window p;
    Window* children;
    
    XQueryTree(g_display, g_root_window, &r, &p, &children, &nchild);
    for (i = 0; i < nchild; i++)
    {
        XWindowAttributes window_attributes;
        XGetWindowAttributes(g_display, children[i], &window_attributes);
        if (!window_attributes.override_redirect)
        {
            if (window_attributes.map_state == IsViewable)
            {
                rail_win_set_state(children[i], 0x0); /* WithdrawnState */
                rail_create_window(children[i], g_root_window);
                rail_win_set_state(children[i], 0x1); /* NormalState */
                rail_win_send_text(children[i]);
            }
        }
    }
    XFree(children);
    return 0;
}

/*****************************************************************************/
static int APP_CC
rail_process_system_param(struct stream *s, int size)
{
    int system_param;

    LOG(10, ("chansrv::rail_process_system_param:"));
    in_uint32_le(s, system_param);
    LOG(10, ("  system_param 0x%8.8x", system_param));
    /*
     * Ask client to re-create the existing rail windows. This is supposed
     * to be done after handshake and client is initialised properly, we
     * consider client is ready when it sends "SET_WORKAREA" sysparam.
     */
    if (system_param == 0x0000002F) /*SPI_SET_WORK_AREA*/
    {
        LOG(10, ("  restore rail windows"));
        rail_restore_windows();
    }
    return 0;
}

/*****************************************************************************/
static int APP_CC
rail_get_property(Display* display, Window target, Atom type, Atom property,
                                 unsigned char** data, unsigned long* count)
{
    Atom atom_return;
    int size;
    unsigned long nitems, bytes_left;
    char* prop_name;
    
    int ret = XGetWindowProperty(display, target, property,
                                 0l, 1l, False,
                                 type, &atom_return, &size,
                                 &nitems, &bytes_left, data);
    if ((ret != Success || nitems < 1) && atom_return == None)
    {
        prop_name = XGetAtomName(g_display, property);
        LOG(10, ("  rail_get_property %s: failed", prop_name));
        XFree(prop_name);
        return 1;
    }
    
    if (bytes_left != 0)
    {
        XFree(*data);
        unsigned long remain = ((size / 8) * nitems) + bytes_left;
        ret = XGetWindowProperty(g_display, target,
                                 property, 0l, remain, False,
                                 atom_return, &atom_return, &size,
                                 &nitems, &bytes_left, data);
        if (ret != Success)
        {
            return 1;
        }
    }
    
    *count = nitems;
    return 0;
}

/*****************************************************************************/
static int APP_CC
rail_win_get_state(Window win)
{
    unsigned long nitems = 0;
    int rv = -1;
    char* data = 0;
    
    rail_get_property(g_display, win, g_wm_state, g_wm_state,
                      (unsigned char **)&data,
                      &nitems);
    
    if (data || nitems > 0)
    {
        rv = *(unsigned long *)data;
        XFree(data);
        LOG(10, ("  rail_win_get_state: %d", rv));
    }
    
    return rv;
}

/*****************************************************************************/
static int APP_CC
rail_win_set_state(Window win, unsigned long state)
{
    int old_state;
    unsigned long data[2] = { state, None };
    
    LOG(10, ("  rail_win_set_state: %d", state));
    /* check whether WM_STATE exists */
    old_state = rail_win_get_state(win);
    if (old_state == -1)
    {
        /* create WM_STATE property */
        XChangeProperty(g_display, win, g_wm_state, g_wm_state, 32, PropModeAppend,
                        (unsigned char *)data, 2);
        LOG(10, ("  rail_win_set_state: create WM_STATE property"));
    }
    else
    {
        XChangeProperty(g_display, win, g_wm_state, g_wm_state, 32, PropModeReplace,
                        (unsigned char *)data, 2);
    }
    return 0;
}

/*****************************************************************************/
static int APP_CC
rail_win_get_text(Window win, char **data)
{
    int ret = 0;
    int i = 0;
    unsigned long nitems = 0;
    
    ret = rail_get_property(g_display, win, g_utf8_string, g_net_wm_name,
                      (unsigned char **)data, &nitems);
    if (ret != 0)
    {
        /* _NET_WM_NAME isn't set, use WM_NAME (XFetchName) instead */
        XFetchName(g_display, win, data);
    }
    
    if (data)
    {
        char *ptr = *data;
        for (; ptr != NULL; i++)
        {
            if (ptr[i] == '\0')
            {
                break;
            }
        }
    }
    
    return i;
}

/******************************************************************************/
static int APP_CC
rail_minmax_window(int window_id, int max)
{
    LOG(10, ("chansrv::rail_minmax_window 0x%8.8x:", window_id));
    if (max)
    {
        
    } else {
        XUnmapWindow(g_display, window_id);
        /* change window state to IconicState (3) */
        rail_win_set_state(window_id, 0x3);
        /*
         * TODO dismiss popups opened so far
         */
    }
    return 0;
}

/*****************************************************************************/
static int APP_CC
rail_restore_window(int window_id)
{
    XWindowAttributes window_attributes;
    
    LOG(10, ("chansrv::rail_restore_window 0x%8.8x:", window_id));
    XGetWindowAttributes(g_display, window_id, &window_attributes);
    if (window_attributes.map_state != IsViewable)
    {
        XMapWindow(g_display, window_id);
    }
    LOG(10, ("chansrv::rail_process_activate: calling XRaiseWindow 0x%8.8x", window_id));
    XRaiseWindow(g_display, window_id);
    LOG(10, ("chansrv::rail_process_activate: calling XSetInputFocus 0x%8.8x", window_id));
    XSetInputFocus(g_display, window_id, RevertToParent, CurrentTime);
    
    return 0;
}

/*****************************************************************************/
static int APP_CC
rail_process_system_command(struct stream *s, int size)
{
    int window_id;
    int command;

    LOG(10, ("chansrv::rail_process_system_command:"));
    in_uint32_le(s, window_id);
    in_uint16_le(s, command);

    switch (command)
    {
        case SC_SIZE:
            LOG(10, ("  window_id 0x%8.8x SC_SIZE", window_id));
            break;
        case SC_MOVE:
            LOG(10, ("  window_id 0x%8.8x SC_MOVE", window_id));
            break;
        case SC_MINIMIZE:
            LOG(10, ("  window_id 0x%8.8x SC_MINIMIZE", window_id));
            rail_minmax_window(window_id, 0);
            break;
        case SC_MAXIMIZE:
            LOG(10, ("  window_id 0x%8.8x SC_MAXIMIZE", window_id));
            break;
        case SC_CLOSE:
            LOG(10, ("  window_id 0x%8.8x SC_CLOSE", window_id));
            rail_close_window(window_id);
            break;
        case SC_KEYMENU:
            LOG(10, ("  window_id 0x%8.8x SC_KEYMENU", window_id));
            break;
        case SC_RESTORE:
            LOG(10, ("  window_id 0x%8.8x SC_RESTORE", window_id));
            rail_restore_window(window_id);
            break;
        case SC_DEFAULT:
            LOG(10, ("  window_id 0x%8.8x SC_DEFAULT", window_id));
            break;
        default:
            LOG(10, ("  window_id 0x%8.8x unknown command command %d",
                     window_id, command));
            break;
    }

    return 0;
}

/*****************************************************************************/
static int APP_CC
rail_process_handshake(struct stream *s, int size)
{
    int build_number;

    LOG(10, ("chansrv::rail_process_handshake:"));
    in_uint32_le(s, build_number);
    LOG(10, ("  build_number 0x%8.8x", build_number));
    return 0;
}

/*****************************************************************************/
static int APP_CC
rail_process_notify_event(struct stream *s, int size)
{
    int window_id;
    int notify_id;
    int message;

    LOG(10, ("chansrv::rail_process_notify_event:"));
    in_uint32_le(s, window_id);
    in_uint32_le(s, notify_id);
    in_uint32_le(s, message);
    LOG(10, ("  window_id 0x%8.8x notify_id 0x%8.8x message 0x%8.8x",
             window_id, notify_id, message));
    return 0;
}

/*****************************************************************************/
static int APP_CC
rail_process_window_move(struct stream *s, int size)
{
    int window_id;
    int left;
    int top;
    int right;
    int bottom;
    tsi16 si16;
    struct rail_window_data* rwd;

    LOG(10, ("chansrv::rail_process_window_move:"));
    in_uint32_le(s, window_id);
    in_uint16_le(s, si16);
    left = si16;
    in_uint16_le(s, si16);
    top = si16;
    in_uint16_le(s, si16);
    right = si16;
    in_uint16_le(s, si16);
    bottom = si16;
    LOG(10, ("  window_id 0x%8.8x left %d top %d right %d bottom %d width %d height %d",
             window_id, left, top, right, bottom, right - left, bottom - top));
    XMoveResizeWindow(g_display, window_id, left, top, right - left, bottom - top);
    rwd = (struct rail_window_data*)
    g_malloc(sizeof(struct rail_window_data), 1);
    rwd->x = left;
    rwd->y = top;
    rwd->width = right - left;
    rwd->height = bottom - top;
    rail_set_window_data(window_id, rwd);
    g_free(rwd);
    return 0;
}

/*****************************************************************************/
static int APP_CC
rail_process_local_move_size(struct stream *s, int size)
{
    int window_id;
    int is_move_size_start;
    int move_size_type;
    int pos_x;
    int pos_y;
    tsi16 si16;

    LOG(10, ("chansrv::rail_process_local_move_size:"));
    in_uint32_le(s, window_id);
    in_uint16_le(s, is_move_size_start);
    in_uint16_le(s, move_size_type);
    in_uint16_le(s, si16);
    pos_x = si16;
    in_uint16_le(s, si16);
    pos_y = si16;
    LOG(10, ("  window_id 0x%8.8x is_move_size_start %d move_size_type %d "
             "pos_x %d pos_y %d", window_id, is_move_size_start, move_size_type,
             pos_x, pos_y));
    return 0;
}

/*****************************************************************************/
/* server to client only */
static int APP_CC
rail_process_min_max_info(struct stream *s, int size)
{
    LOG(10, ("chansrv::rail_process_min_max_info:"));
    return 0;
}

/*****************************************************************************/
static int APP_CC
rail_process_client_status(struct stream *s, int size)
{
    int flags;

    LOG(10, ("chansrv::rail_process_client_status:"));
    in_uint32_le(s, flags);
    LOG(10, ("  flags 0x%8.8x", flags));
    return 0;
}

/*****************************************************************************/
static int APP_CC
rail_process_sys_menu(struct stream *s, int size)
{
    int window_id;
    int left;
    int top;
    tsi16 si16;

    LOG(10, ("chansrv::rail_process_sys_menu:"));
    in_uint32_le(s, window_id);
    in_uint16_le(s, si16);
    left = si16;
    in_uint16_le(s, si16);
    top = si16;
    LOG(10, ("  window_id 0x%8.8x left %d top %d", window_id, left, top));
    return 0;
}

/*****************************************************************************/
static int APP_CC
rail_process_lang_bar_info(struct stream *s, int size)
{
    int language_bar_status;

    LOG(10, ("chansrv::rail_process_lang_bar_info:"));
    in_uint32_le(s, language_bar_status);
    LOG(10, ("  language_bar_status 0x%8.8x", language_bar_status));
    return 0;
}

/*****************************************************************************/
static int APP_CC
rail_process_appid_req(struct stream *s, int size)
{
    LOG(10, ("chansrv::rail_process_appid_req:"));
    return 0;
}

/*****************************************************************************/
static int APP_CC
rail_process_appid_resp(struct stream *s, int size)
{
    LOG(10, ("chansrv::rail_process_appid_resp:"));
    return 0;
}

/*****************************************************************************/
/* server to client only */
static int APP_CC
rail_process_exec_result(struct stream *s, int size)
{
    LOG(10, ("chansrv::rail_process_exec_result:"));
    return 0;
}

/*****************************************************************************/
/* data in from client ( client -> xrdp -> chansrv ) */
int APP_CC
rail_data_in(struct stream *s, int chan_id, int chan_flags, int length,
             int total_length)
{
    int code;
    int size;

    LOG(10, ("chansrv::rail_data_in:"));
    in_uint8(s, code);
    in_uint8s(s, 1);
    in_uint16_le(s, size);

    switch (code)
    {
        case TS_RAIL_ORDER_EXEC: /* 1 */
            rail_process_exec(s, size);
            break;
        case TS_RAIL_ORDER_ACTIVATE: /* 2 */
            rail_process_activate(s, size);
            break;
        case TS_RAIL_ORDER_SYSPARAM: /* 3 */
            rail_process_system_param(s, size);
            break;
        case TS_RAIL_ORDER_SYSCOMMAND: /* 4 */
            rail_process_system_command(s, size);
            break;
        case TS_RAIL_ORDER_HANDSHAKE: /* 5 */
            rail_process_handshake(s, size);
            break;
        case TS_RAIL_ORDER_NOTIFY_EVENT: /* 6 */
            rail_process_notify_event(s, size);
            break;
        case TS_RAIL_ORDER_WINDOWMOVE: /* 8 */
            rail_process_window_move(s, size);
            break;
        case TS_RAIL_ORDER_LOCALMOVESIZE: /* 9 */
            rail_process_local_move_size(s, size);
            break;
        case TS_RAIL_ORDER_MINMAXINFO: /* 10 */
            rail_process_min_max_info(s, size);
            break;
        case TS_RAIL_ORDER_CLIENTSTATUS: /* 11 */
            rail_process_client_status(s, size);
            break;
        case TS_RAIL_ORDER_SYSMENU: /* 12 */
            rail_process_sys_menu(s, size);
            break;
        case TS_RAIL_ORDER_LANGBARINFO: /* 13 */
            rail_process_lang_bar_info(s, size);
            break;
        case TS_RAIL_ORDER_GET_APPID_REQ: /* 14 */
            rail_process_appid_req(s, size);
            break;
        case TS_RAIL_ORDER_GET_APPID_RESP: /* 15 */
            rail_process_appid_resp(s, size);
            break;
        case TS_RAIL_ORDER_EXEC_RESULT: /* 128 */
            rail_process_exec_result(s, size);
            break;
        default:
            LOG(10, ("rail_data_in: unknown code %d size %d", code, size));
            break;
    }

    XFlush(g_display);
    return  0;
}

/*****************************************************************************/
/* returns 0, event handled, 1 unhandled */
static int APP_CC
rail_win_send_text(Window win)
{
    char* data = 0;
    struct stream* s;
    int len = 0;
    int flags;
    struct rail_window_data* rwd;
    
    len = rail_win_get_text(win, &data);
    rwd = rail_get_window_data_safe(win);
    if (rwd != 0)
    {
        if (data != 0)
        {
            if (rwd->valid & RWD_TITLE)
            {
                if (g_strncmp(rwd->title, data, 63) == 0)
                {
                    LOG(0, ("chansrv::rail_win_send_text: skipping, title not changed"));
                    XFree(data);
                    XFree(rwd);
                    return 0;
                }
            }
        }
    }
    else
    {
        LOG(0, ("chansrv::rail_win_send_text: error rail_get_window_data_safe failed"));
        return 1;
    }
    if (data && len > 0) {
        LOG(10, ("chansrv::rail_win_send_text: 0x%8.8x text %s length %d",
                 win, data, len));
        make_stream(s);
        init_stream(s, 1024);
        flags = WINDOW_ORDER_TYPE_WINDOW | WINDOW_ORDER_FIELD_TITLE;
        out_uint32_le(s, 8); /* update title info */
        out_uint32_le(s, win); /* window id */
        out_uint32_le(s, flags); /* flags */
        out_uint32_le(s, len); /* title size */
        out_uint8a(s, data, len); /* title */
        s_mark_end(s);
        send_rail_drawing_orders(s->data, (int)(s->end - s->data));
        free_stream(s);
        XFree(data);
        /* update rail window data */
        rwd->valid |= RWD_TITLE;
        g_strncpy(rwd->title, data, 63);
        rail_set_window_data(win, rwd);
    }
    XFree(rwd);
    return 0;
}

/*****************************************************************************/
static int APP_CC
rail_destroy_window(Window window_id)
{
    struct stream *s;
    
    LOG(10, ("chansrv::rail_destroy_window 0x%8.8x", window_id));
    make_stream(s);
    init_stream(s, 1024);

    out_uint32_le(s, 4); /* destroy_window */
    out_uint32_le(s, window_id);
    s_mark_end(s);
    send_rail_drawing_orders(s->data, (int)(s->end - s->data));
    free_stream(s);

    return 0;
}

/*****************************************************************************/
static int APP_CC
rail_show_window(Window window_id, int show_state)
{
    int flags;
    struct stream* s;

    LOG(10, ("chansrv::rail_show_window 0x%8.8x 0x%x", window_id, show_state));
    make_stream(s);
    init_stream(s, 1024);

    flags = WINDOW_ORDER_TYPE_WINDOW | WINDOW_ORDER_FIELD_SHOW;
    out_uint32_le(s, 6); /* show_window */
    out_uint32_le(s, window_id); /* window_id */
    out_uint32_le(s, flags); /* flags */
    out_uint32_le(s, show_state); /* show_state */
    s_mark_end(s);
    send_rail_drawing_orders(s->data, (int)(s->end - s->data));
    free_stream(s);
    return 0;
}

/*****************************************************************************/
static int APP_CC
rail_create_window(Window window_id, Window owner_id)
{
    int x;
    int y;
    tui32 width;
    tui32 height;
    tui32 border;
    Window root;
    tui32 depth;
    char* title_bytes = 0;
    int title_size = 0;
    XWindowAttributes attributes;
    int style;
    int ext_style;
    int num_window_rects = 1;
    int num_visibility_rects = 1;
    int i = 0;

    int flags;
    int index;
    Window transient_for = 0;
    struct rail_window_data* rwd;
    struct stream* s;

    LOG(10, ("chansrv::rail_create_window 0x%8.8x", window_id));

    rwd = rail_get_window_data_safe(window_id);
    if (rwd == 0)
    {
        LOG(0, ("chansrv::rail_create_window: error rail_get_window_data_safe failed"));
        return 0;
    }
    XGetGeometry(g_display, window_id, &root, &x, &y, &width, &height,
                 &border, &depth);
    XGetWindowAttributes(g_display, window_id, &attributes);

    LOG(10, ("  x %d y %d width %d height %d border_width %d", x, y, width,
             height, border));

    index = list_index_of(g_window_list, window_id);
    if (index == -1)
    {
        LOG(10, ("  create new window"));
        flags = WINDOW_ORDER_TYPE_WINDOW | WINDOW_ORDER_STATE_NEW;
        list_add_item(g_window_list, window_id);
    }
    else
    {
        LOG(10, ("  update existing window"));
        flags = WINDOW_ORDER_TYPE_WINDOW;
    }

    title_size = rail_win_get_text(window_id, &title_bytes);

    XGetTransientForHint(g_display, window_id, &transient_for);
    
    if (attributes.override_redirect)
    {
        style = RAIL_STYLE_TOOLTIP;
        ext_style = RAIL_EXT_STYLE_TOOLTIP;
    }
    else if (transient_for > 0)
    {
        style = RAIL_STYLE_DIALOG;
        ext_style = RAIL_EXT_STYLE_DIALOG;
        owner_id = transient_for;
    }
    else
    {
        style = RAIL_STYLE_NORMAL;
        ext_style = RAIL_EXT_STYLE_NORMAL;
    }

    make_stream(s);
    init_stream(s, 1024);
    
    out_uint32_le(s, 2); /* create_window */
    out_uint32_le(s, window_id); /* window_id */
    out_uint32_le(s, owner_id); /* owner_window_id */
    flags |= WINDOW_ORDER_FIELD_OWNER;
    out_uint32_le(s, style); /* style */
    out_uint32_le(s, ext_style); /* extended_style */
    flags |= WINDOW_ORDER_FIELD_STYLE;
    out_uint32_le(s, 0x05); /* show_state */
    flags |= WINDOW_ORDER_FIELD_SHOW;
    if (title_size > 0)
    {
        out_uint16_le(s, title_size); /* title_size */
        out_uint8a(s, title_bytes, title_size); /* title */
        rwd->valid |= RWD_TITLE;
        g_strncpy(rwd->title, title_bytes, 63);
    }
    else
    {
        out_uint16_le(s, 5); /* title_size */
        out_uint8a(s, "title", 5); /* title */
        rwd->valid |= RWD_TITLE;
        rwd->title[0] = 0;
    }
    LOG(10, ("  set title info %d", title_size));
    flags |= WINDOW_ORDER_FIELD_TITLE;
    out_uint32_le(s, 0); /* client_offset_x */
    out_uint32_le(s, 0); /* client_offset_y */
    flags |= WINDOW_ORDER_FIELD_CLIENT_AREA_OFFSET;
    out_uint32_le(s, width); /* client_area_width */
    out_uint32_le(s, height); /* client_area_height */
    flags |= WINDOW_ORDER_FIELD_CLIENT_AREA_SIZE;
    out_uint32_le(s, 0); /* rp_content */
    out_uint32_le(s, g_root_window); /* root_parent_handle */
    flags |= WINDOW_ORDER_FIELD_ROOT_PARENT;
    out_uint32_le(s, x); /* window_offset_x */
    out_uint32_le(s, y); /* window_offset_y */
    flags |= WINDOW_ORDER_FIELD_WND_OFFSET;
    out_uint32_le(s, 0); /* window_client_delta_x */
    out_uint32_le(s, 0); /* window_client_delta_y */
    flags |= WINDOW_ORDER_FIELD_WND_CLIENT_DELTA;
    out_uint32_le(s, width); /* window_width */
    out_uint32_le(s, height); /* window_height */
    flags |= WINDOW_ORDER_FIELD_WND_SIZE;
    out_uint16_le(s, num_window_rects); /* num_window_rects */
    for (i = 0; i < num_window_rects; i++)
    {
        out_uint16_le(s, 0); /* left */
        out_uint16_le(s, 0); /* top */
        out_uint16_le(s, width); /* right */
        out_uint16_le(s, height); /* bottom */
    }
    flags |= WINDOW_ORDER_FIELD_WND_RECTS;
    out_uint32_le(s, x); /* visible_offset_x */
    out_uint32_le(s, y); /* visible_offset_y */
    flags |= WINDOW_ORDER_FIELD_VIS_OFFSET;
    out_uint16_le(s, num_visibility_rects); /* num_visibility_rects */
    for (i = 0; i < num_visibility_rects; i++)
    {
        out_uint16_le(s, 0); /* left */
        out_uint16_le(s, 0); /* top */
        out_uint16_le(s, width); /* right */
        out_uint16_le(s, height); /* bottom */
    }
    flags |= WINDOW_ORDER_FIELD_VISIBILITY;
    out_uint32_le(s, flags); /*flags*/

    s_mark_end(s);
    send_rail_drawing_orders(s->data, (int)(s->end - s->data));
    free_stream(s);
    XFree(title_bytes);
    rail_set_window_data(window_id, rwd);
    XFree(rwd);
    return 0;
}

/*****************************************************************************/
/* returns 0, event handled, 1 unhandled */
int APP_CC
rail_configure_request_window(XConfigureRequestEvent* config)
{
    int num_window_rects = 1;
    int num_visibility_rects = 1;
    int i = 0;
    int flags;
    int index;
    int window_id;
    int mask;
    int resized = 0;
    struct rail_window_data* rwd;
    
    struct stream* s;
    
    window_id = config->window;
    mask = config->value_mask;
    LOG(10, ("chansrv::rail_configure_request_window: mask %d", mask));
    rwd = rail_get_window_data(window_id);
    if (rwd == 0)
    {
        rwd = (struct rail_window_data*)g_malloc(sizeof(struct rail_window_data), 1);
        rwd->x = config->x;
        rwd->y = config->y;
        rwd->width = config->width;
        rwd->height = config->height;
        rwd->valid |= RWD_X | RWD_Y | RWD_WIDTH | RWD_HEIGHT;
        rail_set_window_data(window_id, rwd);
        g_free(rwd);
        return 0;
    }
    if (!resized)
    {
        if (mask & CWX)
        {
            if (rwd->valid & RWD_X)
            {
                if (rwd->x != config->x)
                {
                    resized = 1;
                    rwd->x = config->x;
                }
            }
            else
            {
                resized = 1;
                rwd->x = config->x;
                rwd->valid |= RWD_X;
            }
        }
    }
    if (!resized)
    {
        if (mask & CWY)
        {
            if (rwd->valid & RWD_Y)
            {
                if (rwd->y != config->y)
                {
                    resized = 1;
                    rwd->y = config->y;
                }
            }
            else
            {
                resized = 1;
                rwd->y = config->y;
                rwd->valid |= RWD_Y;
            }
        }
    }
    if (!resized)
    {
        if (mask & CWWidth)
        {
            if (rwd->valid & RWD_WIDTH)
            {
                if (rwd->width != config->width)
                {
                    resized = 1;
                    rwd->width = config->width;
                }
            }
            else
            {
                resized = 1;
                rwd->width = config->width;
                rwd->valid |= RWD_WIDTH;
            }
        }
    }
    if (!resized)
    {
        if (mask & CWHeight)
        {
            if (rwd->valid & RWD_HEIGHT)
            {
                if (rwd->height != config->height)
                {
                    resized = 1;
                    rwd->height = config->height;
                }
            }
            else
            {
                resized = 1;
                rwd->height = config->height;
                rwd->valid |= RWD_HEIGHT;
            }
        }
    }
    if (resized)
    {
        rail_set_window_data(window_id, rwd);
        XFree(rwd);
    }
    else
    {
        XFree(rwd);
        return 0;
    }
    
    LOG(10, ("chansrv::rail_configure_request_window: 0x%8.8x", window_id));
    
    
    LOG(10, ("  x %d y %d width %d height %d border_width %d", config->x,
             config->y, config->width, config->height, config->border_width));
    
    index = list_index_of(g_window_list, window_id);
    if (index == -1)
    {
        /* window isn't mapped yet */
        LOG(0, ("chansrv::rail_configure_request_window: window not mapped"));
        return 0;
    }
    
    flags = WINDOW_ORDER_TYPE_WINDOW;
    
    make_stream(s);
    init_stream(s, 1024);
    
    out_uint32_le(s, 10); /* configure_window */
    out_uint32_le(s, window_id); /* window_id */
    
    out_uint32_le(s, 0); /* client_offset_x */
    out_uint32_le(s, 0); /* client_offset_y */
    flags |= WINDOW_ORDER_FIELD_CLIENT_AREA_OFFSET;
    out_uint32_le(s, config->width); /* client_area_width */
    out_uint32_le(s, config->height); /* client_area_height */
    flags |= WINDOW_ORDER_FIELD_CLIENT_AREA_SIZE;
    out_uint32_le(s, 0); /* rp_content */
    out_uint32_le(s, g_root_window); /* root_parent_handle */
    flags |= WINDOW_ORDER_FIELD_ROOT_PARENT;
    out_uint32_le(s, config->x); /* window_offset_x */
    out_uint32_le(s, config->y); /* window_offset_y */
    flags |= WINDOW_ORDER_FIELD_WND_OFFSET;
    out_uint32_le(s, 0); /* window_client_delta_x */
    out_uint32_le(s, 0); /* window_client_delta_y */
    flags |= WINDOW_ORDER_FIELD_WND_CLIENT_DELTA;
    out_uint32_le(s, config->width); /* window_width */
    out_uint32_le(s, config->height); /* window_height */
    flags |= WINDOW_ORDER_FIELD_WND_SIZE;
    out_uint16_le(s, num_window_rects); /* num_window_rects */
    for (i = 0; i < num_window_rects; i++)
    {
        out_uint16_le(s, 0); /* left */
        out_uint16_le(s, 0); /* top */
        out_uint16_le(s, config->width); /* right */
        out_uint16_le(s, config->height); /* bottom */
    }
    flags |= WINDOW_ORDER_FIELD_WND_RECTS;
    out_uint32_le(s, config->x); /* visible_offset_x */
    out_uint32_le(s, config->y); /* visible_offset_y */
    flags |= WINDOW_ORDER_FIELD_VIS_OFFSET;
    out_uint16_le(s, num_visibility_rects); /* num_visibility_rects */
    for (i = 0; i < num_visibility_rects; i++)
    {
        out_uint16_le(s, 0); /* left */
        out_uint16_le(s, 0); /* top */
        out_uint16_le(s, config->width); /* right */
        out_uint16_le(s, config->height); /* bottom */
    }
    flags |= WINDOW_ORDER_FIELD_VISIBILITY;
    out_uint32_le(s, flags); /*flags*/
    
    s_mark_end(s);
    send_rail_drawing_orders(s->data, (int)(s->end - s->data));
    free_stream(s);
    return 0;
}

/*****************************************************************************/
/* returns 0, event handled, 1 unhandled */
int APP_CC
rail_configure_window(XConfigureEvent *config)
{
    int num_window_rects = 1;
    int num_visibility_rects = 1;
    int i = 0;
    int flags;
    int index;
    int window_id;
    
    struct stream* s;
    
    window_id = config->window;
    
    LOG(10, ("chansrv::rail_configure_window 0x%8.8x", window_id));
    
    
    LOG(10, ("  x %d y %d width %d height %d border_width %d", config->x,
             config->y, config->width, config->height, config->border_width));
    
    index = list_index_of(g_window_list, window_id);
    if (index == -1)
    {
        /* window isn't mapped yet */
        return 0;
    }
    
    flags = WINDOW_ORDER_TYPE_WINDOW;
    
    make_stream(s);
    init_stream(s, 1024);
    
    out_uint32_le(s, 10); /* configure_window */
    out_uint32_le(s, window_id); /* window_id */
    
    out_uint32_le(s, 0); /* client_offset_x */
    out_uint32_le(s, 0); /* client_offset_y */
    flags |= WINDOW_ORDER_FIELD_CLIENT_AREA_OFFSET;
    out_uint32_le(s, config->width); /* client_area_width */
    out_uint32_le(s, config->height); /* client_area_height */
    flags |= WINDOW_ORDER_FIELD_CLIENT_AREA_SIZE;
    out_uint32_le(s, 0); /* rp_content */
    out_uint32_le(s, g_root_window); /* root_parent_handle */
    flags |= WINDOW_ORDER_FIELD_ROOT_PARENT;
    out_uint32_le(s, config->x); /* window_offset_x */
    out_uint32_le(s, config->y); /* window_offset_y */
    flags |= WINDOW_ORDER_FIELD_WND_OFFSET;
    out_uint32_le(s, 0); /* window_client_delta_x */
    out_uint32_le(s, 0); /* window_client_delta_y */
    flags |= WINDOW_ORDER_FIELD_WND_CLIENT_DELTA;
    out_uint32_le(s, config->width); /* window_width */
    out_uint32_le(s, config->height); /* window_height */
    flags |= WINDOW_ORDER_FIELD_WND_SIZE;
    out_uint16_le(s, num_window_rects); /* num_window_rects */
    for (i = 0; i < num_window_rects; i++)
    {
        out_uint16_le(s, 0); /* left */
        out_uint16_le(s, 0); /* top */
        out_uint16_le(s, config->width); /* right */
        out_uint16_le(s, config->height); /* bottom */
    }
    flags |= WINDOW_ORDER_FIELD_WND_RECTS;
    out_uint32_le(s, config->x); /* visible_offset_x */
    out_uint32_le(s, config->y); /* visible_offset_y */
    flags |= WINDOW_ORDER_FIELD_VIS_OFFSET;
    out_uint16_le(s, num_visibility_rects); /* num_visibility_rects */
    for (i = 0; i < num_visibility_rects; i++)
    {
        out_uint16_le(s, 0); /* left */
        out_uint16_le(s, 0); /* top */
        out_uint16_le(s, config->width); /* right */
        out_uint16_le(s, config->height); /* bottom */
    }
    flags |= WINDOW_ORDER_FIELD_VISIBILITY;
    out_uint32_le(s, flags); /*flags*/
    
    s_mark_end(s);
    send_rail_drawing_orders(s->data, (int)(s->end - s->data));
    free_stream(s);
    return 0;
}

/*****************************************************************************/
/* returns 0, event handled, 1 unhandled */
int APP_CC
rail_xevent(void *xevent)
{
    XEvent *lxevent;
    XEvent lastevent;
    XWindowChanges xwc;
    int rv;
    int index;
    XWindowAttributes wnd_attributes;
    char* prop_name;

    LOG(10, ("chansrv::rail_xevent:"));

    if (!g_rail_up)
    {
        return 1;
    }

    rv = 1;
    lxevent = (XEvent *)xevent;

    switch (lxevent->type)
    {
        case PropertyNotify:
            prop_name = XGetAtomName(g_display, lxevent->xproperty.atom);
            LOG(10, ("  got PropertyNotify window_id 0x%8.8x %s state new %d",
                     lxevent->xproperty.window, prop_name,
                     lxevent->xproperty.state == PropertyNewValue));
            if (g_strcmp(prop_name, "WM_NAME") == 0 ||
                g_strcmp(prop_name, "_NET_WM_NAME") == 0)
            {
                XGetWindowAttributes(g_display, lxevent->xproperty.window, &wnd_attributes);
                if (wnd_attributes.map_state == IsViewable)
                {
                    rail_win_send_text(lxevent->xproperty.window);
                    rv = 0;
                }
            }
            XFree(prop_name);
            break;
        case ConfigureRequest:
            LOG(10, ("  got ConfigureRequest window_id 0x%8.8x", lxevent->xconfigurerequest.window));
            g_memset(&xwc, 0, sizeof(xwc));
            xwc.x = lxevent->xconfigurerequest.x;
            xwc.y = lxevent->xconfigurerequest.y;
            xwc.width = lxevent->xconfigurerequest.width;
            xwc.height = lxevent->xconfigurerequest.height;
            xwc.border_width = lxevent->xconfigurerequest.border_width;
            xwc.sibling = lxevent->xconfigurerequest.above;
            xwc.stack_mode = lxevent->xconfigurerequest.detail;
            XConfigureWindow(g_display,
                             lxevent->xconfigurerequest.window,
                             lxevent->xconfigurerequest.value_mask,
                             &xwc);
            rail_configure_request_window(&(lxevent->xconfigurerequest));
            rv = 0;
            break;

        case CreateNotify:
            LOG(10, (" got CreateNotify window 0x%8.8x parent 0x%8.8x",
                     lxevent->xcreatewindow.window, lxevent->xcreatewindow.parent));
            break;

        case DestroyNotify:
            LOG(10, ("  got DestroyNotify 0x%8.8x", lxevent->xdestroywindow.window));
            index = list_index_of(g_window_list, lxevent->xdestroywindow.window);
            if (index >= 0)
            {
                rail_destroy_window(lxevent->xdestroywindow.window);
                list_remove_item(g_window_list, index);
            }
            rv = 0;
            break;
            
        case MapRequest:
            LOG(10, ("  got MapRequest window 0x%8.8x", lxevent->xmaprequest.window));
            XSelectInput(g_display, lxevent->xmaprequest.window,
                         PropertyChangeMask | StructureNotifyMask);
            XMapWindow(g_display, lxevent->xmaprequest.window);
            break;

        case MapNotify:
            LOG(10, ("  got MapNotify window 0x%8.8x event 0x%8.8x",
                     lxevent->xmap.window, lxevent->xmap.event));
            if (lxevent->xmap.window != lxevent->xmap.event)
            {
                XGetWindowAttributes(g_display, lxevent->xmap.window, &wnd_attributes);
                if (wnd_attributes.map_state == IsViewable)
                {
                    rail_create_window(lxevent->xmap.window, lxevent->xmap.event);
                    if (!wnd_attributes.override_redirect)
                    {
                        rail_win_set_state(lxevent->xmap.window, 0x1); /* NormalState */
                        rail_win_send_text(lxevent->xmap.window);
                    }
                    rv = 0;
                }
            }
            break;

        case UnmapNotify:
            LOG(10, ("  got UnmapNotify 0x%8.8x", lxevent->xunmap.event));
            if (lxevent->xunmap.window != lxevent->xunmap.event &&
                is_window_valid_child_of_root(lxevent->xunmap.window))
            {
                int state = rail_win_get_state(lxevent->xunmap.window);
                index = list_index_of(g_window_list, lxevent->xunmap.window);
                LOG(10, ("  window 0x%8.8x is unmapped", lxevent->xunmap.window));
                if (index >= 0)
                {
#if 0
                    XGetWindowAttributes(g_display, lxevent->xunmap.window, &wnd_attributes);
                    if (wnd_attributes.override_redirect)
                    {
                        LOG(10, ("  hide popup"));
                        rail_show_window(lxevent->xunmap.window, 0x0);
                        rv = 0;
                    }
#else
                    rail_show_window(lxevent->xunmap.window, 0x0);
                    rv = 0;
#endif
                }
            }
            break;

        case ConfigureNotify:
            LOG(10, ("  got ConfigureNotify 0x%8.8x event 0x%8.8x", lxevent->xconfigure.window,
                     lxevent->xconfigure.event));
            rv = 0;
            if (lxevent->xconfigure.event != lxevent->xconfigure.window ||
                lxevent->xconfigure.override_redirect)
            {
                break;
            }
            /* skip dup ConfigureNotify */
            while (XCheckTypedWindowEvent(g_display,
                                          lxevent->xconfigure.window,
                                          ConfigureNotify, &lastevent))
            {
                if (lastevent.xconfigure.event == lastevent.xconfigure.window &&
                    lxevent->xconfigure.override_redirect == 0)
                {
                    lxevent = &lastevent;
                }
            }
#if 0
            rail_configure_window(&(lxevent->xconfigure));
#endif
            break;

        case FocusIn:
            LOG(10, ("  got FocusIn"));
            break;

        case ButtonPress:
            LOG(10, ("  got ButtonPress"));
            break;

        case EnterNotify:
            LOG(10, ("  got EnterNotify"));
            break;

        case LeaveNotify:
            LOG(10, ("  got LeaveNotify"));
            break;

        case ReparentNotify:
            LOG(10, ("  got ReparentNotify window 0x%8.8x parent 0x%8.8x "
                     "event 0x%8.8x x %d y %d overrider redirect %d",
                     lxevent->xreparent.window, lxevent->xreparent.parent,
                     lxevent->xreparent.event, lxevent->xreparent.x,
                     lxevent->xreparent.y, lxevent->xreparent.override_redirect));

            if (lxevent->xreparent.parent != g_root_window)
            {
                index = list_index_of(g_window_list, lxevent->xreparent.window);
                if (index >= 0)
                {
                    rail_destroy_window(lxevent->xreparent.window);
                    list_remove_item(g_window_list, index);
                }
            }
            rv = 0;
            break;
    }

    return rv;
}
