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

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#include <X11/cursorfont.h>
#include "chansrv.h"
#include "rail.h"
#include "xcommon.h"
#include "log.h"
#include "os_calls.h"
#include "string_calls.h"
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
static struct list *g_window_list = 0;

static int g_got_focus = 0;
static int g_focus_counter = 0;
static Window g_focus_win = 0;

static int g_xrr_event_base = 0; /* non zero means we got extension */

static Cursor g_default_cursor = 0;

static char *g_override_window_title = 0;

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
    int title_crc; /* crc of title for compare */
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

static int rail_win_get_state(Window win);
static int rail_create_window(Window window_id, Window owner_id);
static int rail_win_set_state(Window win, unsigned long state);
static int rail_show_window(Window window_id, int show_state);
static int rail_win_send_text(Window win);

/*****************************************************************************/
static int
rail_send_key_esc(int window_id)
{
    XEvent event;

    g_memset(&event, 0, sizeof(event));
    event.type = KeyPress;
    event.xkey.same_screen = True;
    event.xkey.root = g_root_window;
    event.xkey.window = window_id;
    event.xkey.keycode = 9;
    XSendEvent(g_display, window_id, True, 0xfff, &event);
    event.type = KeyRelease;
    XSendEvent(g_display, window_id, True, 0xfff, &event);
    return 0;
}

/*****************************************************************************/
static struct rail_window_data *
rail_get_window_data(Window window)
{
    unsigned int bytes;
    Atom actual_type_return;
    int actual_format_return;
    unsigned long nitems_return;
    unsigned long bytes_after_return;
    unsigned char *prop_return;
    struct rail_window_data *rv;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_get_window_data:");
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
        rv = (struct rail_window_data *)prop_return;
    }
    return rv;
}

/*****************************************************************************/
static int
rail_set_window_data(Window window, struct rail_window_data *rwd)
{
    int bytes;

    bytes = sizeof(struct rail_window_data);
    XChangeProperty(g_display, window, g_rwd_atom, XA_STRING, 8,
                    PropModeReplace, (unsigned char *)rwd, bytes);
    return 0;
}

/*****************************************************************************/
/* get the rail window data, if not exist, try to create it and return */
static struct rail_window_data *
rail_get_window_data_safe(Window window)
{
    struct rail_window_data *rv;

    rv = rail_get_window_data(window);
    if (rv != 0)
    {
        return rv;
    }
    rv = g_new0(struct rail_window_data, 1);
    rail_set_window_data(window, rv);
    g_free(rv);
    return rail_get_window_data(window);
}

/******************************************************************************/
static int
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
static int
rail_send_init(void)
{
    struct stream *s;
    int bytes;
    char *size_ptr;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_send_init:");
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
static int
anotherWMRunning(Display *display, XErrorEvent *xe)
{
    g_rail_running = 0;
    return -1;
}

/******************************************************************************/
static int
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
int
rail_init(void)
{
    LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_init:");
    xcommon_init();

    return 0;
}

/*****************************************************************************/
int
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

int
rail_startup(void)
{
    int dummy;
    int ver_maj;
    int ver_min;
    Status st;

    if (rail_is_another_wm_running())
    {
        LOG(LOG_LEVEL_ERROR, "rail_init: another window manager "
            "is running");
    }

    list_delete(g_window_list);
    g_window_list = list_create();
    rail_send_init();
    g_rail_up = 1;
    g_rwd_atom = XInternAtom(g_display, "XRDP_RAIL_WINDOW_DATA", 0);

    if (!XRRQueryExtension(g_display, &g_xrr_event_base, &dummy))
    {
        g_xrr_event_base = 0;
        LOG(LOG_LEVEL_ERROR, "rail_init: RandR extension not found");
    }

    if (g_xrr_event_base > 0)
    {
        LOG_DEVEL(LOG_LEVEL_INFO, "rail_init: found RandR extension");
        st = XRRQueryVersion(g_display, &ver_maj, &ver_min);
        if (st)
        {
            LOG_DEVEL(LOG_LEVEL_INFO, "rail_init: RandR version major %d minor %d", ver_maj, ver_min);
        }
        XRRSelectInput(g_display, g_root_window, RRScreenChangeNotifyMask);
    }

    if (g_default_cursor == 0)
    {
        g_default_cursor = XCreateFontCursor(g_display, XC_left_ptr);
        XDefineCursor(g_display, g_root_window, g_default_cursor);
    }

    return 0;
}

/*****************************************************************************/
static char *
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
static int
rail_process_exec(struct stream *s, int size)
{
    int flags;
    int ExeOrFileLength;
    int WorkingDirLength;
    int ArgumentsLen;
    char *ExeOrFile;
    char *WorkingDir;
    char *Arguments;

    LOG_DEVEL(LOG_LEVEL_INFO, "chansrv::rail_process_exec:");
    in_uint16_le(s, flags);
    in_uint16_le(s, ExeOrFileLength);
    in_uint16_le(s, WorkingDirLength);
    in_uint16_le(s, ArgumentsLen);
    ExeOrFile = read_uni(s, ExeOrFileLength);
    WorkingDir = read_uni(s, WorkingDirLength);
    Arguments = read_uni(s, ArgumentsLen);
    LOG(LOG_LEVEL_DEBUG, "  flags 0x%8.8x ExeOrFileLength %d WorkingDirLength %d "
        "ArgumentsLen %d ExeOrFile [%s] WorkingDir [%s] "
        "Arguments [%s]", flags, ExeOrFileLength, WorkingDirLength,
        ArgumentsLen, ExeOrFile, WorkingDir, Arguments);

    if (g_strlen(ExeOrFile) > 0)
    {
        rail_startup();

        LOG_DEVEL(LOG_LEVEL_DEBUG, "rail_process_exec: pre");
        /* ask main thread to fork */
        tc_mutex_lock(g_exec_mutex);
        g_exec_name = ExeOrFile;
        g_set_wait_obj(g_exec_event);
        tc_sem_dec(g_exec_sem);
        tc_mutex_unlock(g_exec_mutex);
        LOG_DEVEL(LOG_LEVEL_DEBUG, "rail_process_exec: post");
    }

    g_free(ExeOrFile);
    g_free(WorkingDir);
    g_free(Arguments);
    return 0;
}

/******************************************************************************/
static int
rail_win_popdown(void)
{
    int rv = 0;
    int i;
    unsigned int nchild;
    Window r;
    Window p;
    Window *children;
    XWindowAttributes window_attributes;

    /*
     * Check the tree of current existing X windows and dismiss
     * the managed rail popups by simulating a esc key, so
     * that the requested window can be closed properly.
     */

    XQueryTree(g_display, g_root_window, &r, &p, &children, &nchild);
    for (i = nchild - 1; i >= 0; i--)
    {
        XGetWindowAttributes(g_display, children[i], &window_attributes);
        if (window_attributes.override_redirect &&
                window_attributes.map_state == IsViewable &&
                list_index_of(g_window_list, children[i]) >= 0)
        {
            LOG_DEVEL(LOG_LEVEL_DEBUG, "  dismiss pop up 0x%8.8lx", children[i]);
            rail_send_key_esc(children[i]);
            rv = 1;
        }
    }

    XFree(children);
    return rv;
}

/******************************************************************************/
static int
rail_close_window(int window_id)
{
    XEvent ce;

    LOG_DEVEL(LOG_LEVEL_INFO, "chansrv::rail_close_window:");

    rail_win_popdown();

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
void
my_timeout(void *data)
{
    LOG_DEVEL(LOG_LEVEL_DEBUG, "my_timeout: g_got_focus %d", g_got_focus);
    if (g_focus_counter == (int)(long)data)
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "my_timeout: g_focus_counter %d", g_focus_counter);
        rail_win_popdown();
    }
}

/*****************************************************************************/
static int
rail_process_activate(struct stream *s, int size)
{
    unsigned int window_id;
    int enabled;
    int index;
    XWindowAttributes window_attributes;
    Window transient_for = 0;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_process_activate:");
    in_uint32_le(s, window_id);
    in_uint8(s, enabled);

    index = list_index_of(g_window_list, window_id);
    if (index < 0)
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_process_activate: window 0x%8.8x not in list",
                  window_id);
        return 0;
    }

    g_focus_counter++;
    g_got_focus = enabled;
    LOG_DEVEL(LOG_LEVEL_DEBUG, "  window_id 0x%8.8x enabled %d", window_id, enabled);

    XGetWindowAttributes(g_display, window_id, &window_attributes);

    if (enabled)
    {
        if (g_focus_win == window_id)
        {
            /* In case that window is unmapped upon minimization and not yet mapped*/
            XMapWindow(g_display, window_id);
        }
        else
        {
            rail_win_popdown();
            if (window_attributes.map_state != IsViewable)
            {
                /* In case that window is unmapped upon minimization and not yet mapped */
                XMapWindow(g_display, window_id);
            }
            XGetTransientForHint(g_display, window_id, &transient_for);
            if (transient_for > 0)
            {
                /* Owner window should be raised up as well */
                XRaiseWindow(g_display, transient_for);
            }
            LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_process_activate: calling XRaiseWindow 0x%8.8x", window_id);
            XRaiseWindow(g_display, window_id);
            LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_process_activate: calling XSetInputFocus 0x%8.8x", window_id);
            XSetInputFocus(g_display, window_id, RevertToParent, CurrentTime);
        }
        LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_process_activate: calling XRaiseWindow 0x%8.8x", window_id);
        XRaiseWindow(g_display, window_id);
        LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_process_activate: calling XSetInputFocus 0x%8.8x", window_id);
        XSetInputFocus(g_display, window_id, RevertToParent, CurrentTime);
    }
    else
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "  window attributes: override_redirect %d",
                  window_attributes.override_redirect);
        add_timeout(200, my_timeout, (void *)(long)g_focus_counter);
    }
    return 0;
}

/*****************************************************************************/
static int
rail_select_input(Window window_id)
{
    XSelectInput(g_display, window_id,
                 PropertyChangeMask | StructureNotifyMask |
                 SubstructureNotifyMask | FocusChangeMask |
                 EnterWindowMask | LeaveWindowMask);
    XSync(g_display, 0);
    return 0;
}

/*****************************************************************************/
static int
rail_restore_windows(void)
{
    unsigned int i;
    unsigned int nchild;
    Window r;
    Window p;
    Window *children;

    XQueryTree(g_display, g_root_window, &r, &p, &children, &nchild);
    for (i = 0; i < nchild; i++)
    {
        XWindowAttributes window_attributes;
        XGetWindowAttributes(g_display, children[i], &window_attributes);
        if (!window_attributes.override_redirect)
        {
            rail_select_input(children[i]);
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
static int
rail_process_system_param(struct stream *s, int size)
{
    int system_param;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_process_system_param:");
    in_uint32_le(s, system_param);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "  system_param 0x%8.8x", system_param);
    /*
     * Ask client to re-create the existing rail windows. This is supposed
     * to be done after handshake and client is initialised properly, we
     * consider client is ready when it sends "SET_WORKAREA" sysparam.
     */
    if (system_param == 0x0000002F) /*SPI_SET_WORK_AREA*/
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "  restore rail windows");
        rail_restore_windows();
    }
    return 0;
}

/*****************************************************************************/
static int
rail_get_property(Display *display, Window target, Atom type, Atom property,
                  unsigned char **data, unsigned long *count)
{
    Atom atom_return;
    int size;
    unsigned long nitems, bytes_left;
    char *prop_name;

    int ret = XGetWindowProperty(display, target, property,
                                 0l, 1l, False,
                                 type, &atom_return, &size,
                                 &nitems, &bytes_left, data);
    if ((ret != Success || nitems < 1) && atom_return == None)
    {
        prop_name = XGetAtomName(g_display, property);
        LOG_DEVEL(LOG_LEVEL_DEBUG, "  rail_get_property %s: failed", prop_name);
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
static int
rail_win_get_state(Window win)
{
    unsigned long nitems = 0;
    int rv = -1;
    char *data = 0;

    rail_get_property(g_display, win, g_wm_state, g_wm_state,
                      (unsigned char **)&data,
                      &nitems);

    if (data && nitems > 0)
    {
        rv = *(unsigned long *)data;
        XFree(data);
        LOG_DEVEL(LOG_LEVEL_DEBUG, "  rail_win_get_state: %d", rv);
    }

    return rv;
}

/*****************************************************************************/
static int
rail_win_set_state(Window win, unsigned long state)
{
    int old_state;
    unsigned long data[2] = { state, None };

    LOG_DEVEL(LOG_LEVEL_DEBUG, "  rail_win_set_state: %ld", state);
    /* check whether WM_STATE exists */
    old_state = rail_win_get_state(win);
    if (old_state == -1)
    {
        /* create WM_STATE property */
        XChangeProperty(g_display, win, g_wm_state, g_wm_state, 32, PropModeAppend,
                        (unsigned char *)data, 2);
        LOG_DEVEL(LOG_LEVEL_DEBUG, "  rail_win_set_state: create WM_STATE property");
    }
    else
    {
        XChangeProperty(g_display, win, g_wm_state, g_wm_state, 32, PropModeReplace,
                        (unsigned char *)data, 2);
    }
    return 0;
}

/*****************************************************************************/
/* *data pointer that needs g_free */
static int
rail_win_get_text(Window win, char **data)
{
    int ret = 0;
    int i = 0;
    unsigned long nitems = 0;
    unsigned char *ldata = 0;
    char *lldata = 0;

    if (g_override_window_title != 0)
    {
        *data = g_strdup(g_override_window_title);
        return g_strlen(*data);
    }
    ret = rail_get_property(g_display, win, g_utf8_string, g_net_wm_name,
                            &ldata, &nitems);
    if (ret != 0)
    {
        /* _NET_WM_NAME isn't set, use WM_NAME (XFetchName) instead */
        XFetchName(g_display, win, &lldata);
        *data = g_strdup(lldata);
        i = g_strlen(*data);
        XFree(lldata);
        return i;
    }

    *data = 0;
    if (ldata)
    {
        *data = g_strdup((char *)ldata);
        i = g_strlen(*data);
        XFree(ldata);
        return i;
    }

    return i;
}

/******************************************************************************/
static int
rail_minmax_window(int window_id, int max)
{
    LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_minmax_window 0x%8.8x:", window_id);
    if (max)
    {

    }
    else
    {
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
static int
rail_restore_window(int window_id)
{
    XWindowAttributes window_attributes;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_restore_window 0x%8.8x:", window_id);
    XGetWindowAttributes(g_display, window_id, &window_attributes);
    if (window_attributes.map_state != IsViewable)
    {
        XMapWindow(g_display, window_id);
    }
    LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_process_activate: calling XRaiseWindow 0x%8.8x", window_id);
    XRaiseWindow(g_display, window_id);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_process_activate: calling XSetInputFocus 0x%8.8x", window_id);
    XSetInputFocus(g_display, window_id, RevertToParent, CurrentTime);

    return 0;
}

/*****************************************************************************/
static int
rail_process_system_command(struct stream *s, int size)
{
    int window_id;
    int command;
    int index;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_process_system_command:");
    in_uint32_le(s, window_id);
    in_uint16_le(s, command);

    index = list_index_of(g_window_list, window_id);
    if (index < 0)
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_process_system_command: window 0x%8.8x not in list",
                  window_id);
        return 0;
    }

    switch (command)
    {
        case SC_SIZE:
            LOG_DEVEL(LOG_LEVEL_DEBUG, "  window_id 0x%8.8x SC_SIZE", window_id);
            break;
        case SC_MOVE:
            LOG_DEVEL(LOG_LEVEL_DEBUG, "  window_id 0x%8.8x SC_MOVE", window_id);
            break;
        case SC_MINIMIZE:
            LOG_DEVEL(LOG_LEVEL_DEBUG, "  window_id 0x%8.8x SC_MINIMIZE", window_id);
            rail_minmax_window(window_id, 0);
            break;
        case SC_MAXIMIZE:
            LOG_DEVEL(LOG_LEVEL_DEBUG, "  window_id 0x%8.8x SC_MAXIMIZE", window_id);
            break;
        case SC_CLOSE:
            LOG_DEVEL(LOG_LEVEL_DEBUG, "  window_id 0x%8.8x SC_CLOSE", window_id);
            rail_close_window(window_id);
            break;
        case SC_KEYMENU:
            LOG_DEVEL(LOG_LEVEL_DEBUG, "  window_id 0x%8.8x SC_KEYMENU", window_id);
            break;
        case SC_RESTORE:
            LOG_DEVEL(LOG_LEVEL_DEBUG, "  window_id 0x%8.8x SC_RESTORE", window_id);
            rail_restore_window(window_id);
            break;
        case SC_DEFAULT:
            LOG_DEVEL(LOG_LEVEL_DEBUG, "  window_id 0x%8.8x SC_DEFAULT", window_id);
            break;
        default:
            LOG_DEVEL(LOG_LEVEL_DEBUG, "  window_id 0x%8.8x unknown command command %d",
                      window_id, command);
            break;
    }

    return 0;
}

/*****************************************************************************/
static int
rail_process_handshake(struct stream *s, int size)
{
    int build_number;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_process_handshake:");
    in_uint32_le(s, build_number);
    LOG(LOG_LEVEL_DEBUG, "  build_number 0x%8.8x", build_number);
    return 0;
}

/*****************************************************************************/
static int
rail_process_notify_event(struct stream *s, int size)
{
    int window_id;
    int notify_id;
    int message;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_process_notify_event:");
    in_uint32_le(s, window_id);
    in_uint32_le(s, notify_id);
    in_uint32_le(s, message);
    LOG(LOG_LEVEL_DEBUG, "  window_id 0x%8.8x notify_id 0x%8.8x message 0x%8.8x",
        window_id, notify_id, message);
    return 0;
}

/*****************************************************************************/
static int
rail_process_window_move(struct stream *s, int size)
{
    int window_id;
    int left;
    int top;
    int right;
    int bottom;
    tsi16 si16;
    struct rail_window_data *rwd;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_process_window_move:");
    in_uint32_le(s, window_id);
    in_uint16_le(s, si16);
    left = si16;
    in_uint16_le(s, si16);
    top = si16;
    in_uint16_le(s, si16);
    right = si16;
    in_uint16_le(s, si16);
    bottom = si16;
    LOG_DEVEL(LOG_LEVEL_DEBUG, "  window_id 0x%8.8x left %d top %d right %d bottom %d width %d height %d",
              window_id, left, top, right, bottom, right - left, bottom - top);
    XMoveResizeWindow(g_display, window_id, left, top, right - left, bottom - top);
    rwd = (struct rail_window_data *)
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
static int
rail_process_local_move_size(struct stream *s, int size)
{
    int window_id;
    int is_move_size_start;
    int move_size_type;
    int pos_x;
    int pos_y;
    tsi16 si16;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_process_local_move_size:");
    in_uint32_le(s, window_id);
    in_uint16_le(s, is_move_size_start);
    in_uint16_le(s, move_size_type);
    in_uint16_le(s, si16);
    pos_x = si16;
    in_uint16_le(s, si16);
    pos_y = si16;
    LOG(LOG_LEVEL_DEBUG, "  window_id 0x%8.8x is_move_size_start %d move_size_type %d "
        "pos_x %d pos_y %d", window_id, is_move_size_start, move_size_type,
        pos_x, pos_y);
    return 0;
}

/*****************************************************************************/
/* server to client only */
static int
rail_process_min_max_info(struct stream *s, int size)
{
    LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_process_min_max_info:");
    return 0;
}

/*****************************************************************************/
static int
rail_process_client_status(struct stream *s, int size)
{
    int flags;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_process_client_status:");
    in_uint32_le(s, flags);
    LOG(LOG_LEVEL_DEBUG, "  flags 0x%8.8x", flags);
    return 0;
}

/*****************************************************************************/
static int
rail_process_sys_menu(struct stream *s, int size)
{
    int window_id;
    int left;
    int top;
    tsi16 si16;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_process_sys_menu:");
    in_uint32_le(s, window_id);
    in_uint16_le(s, si16);
    left = si16;
    in_uint16_le(s, si16);
    top = si16;
    LOG(LOG_LEVEL_DEBUG, "  window_id 0x%8.8x left %d top %d", window_id, left, top);
    return 0;
}

/*****************************************************************************/
static int
rail_process_lang_bar_info(struct stream *s, int size)
{
    int language_bar_status;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_process_lang_bar_info:");
    in_uint32_le(s, language_bar_status);
    LOG(LOG_LEVEL_DEBUG, "  language_bar_status 0x%8.8x", language_bar_status);
    return 0;
}

/*****************************************************************************/
static int
rail_process_appid_req(struct stream *s, int size)
{
    LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_process_appid_req:");
    return 0;
}

/*****************************************************************************/
static int
rail_process_appid_resp(struct stream *s, int size)
{
    LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_process_appid_resp:");
    return 0;
}

/*****************************************************************************/
/* server to client only */
static int
rail_process_exec_result(struct stream *s, int size)
{
    LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_process_exec_result:");
    return 0;
}

/*****************************************************************************/
/* data in from client ( client -> xrdp -> chansrv ) */
int
rail_data_in(struct stream *s, int chan_id, int chan_flags, int length,
             int total_length)
{
    int code;
    int size;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_data_in:");
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
            LOG_DEVEL(LOG_LEVEL_DEBUG, "rail_data_in: unknown code %d size %d", code, size);
            break;
    }

    XFlush(g_display);
    return  0;
}

static const unsigned int g_crc_seed = 0xffffffff;
static const unsigned int g_crc_table[256] =
{
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

#define CRC_START(in_crc) (in_crc) = g_crc_seed
#define CRC_PASS(in_pixel, in_crc) \
    (in_crc) = g_crc_table[((in_crc) ^ (in_pixel)) & 0xff] ^ ((in_crc) >> 8)
#define CRC_END(in_crc) (in_crc) = ((in_crc) ^ g_crc_seed)

/*****************************************************************************/
static int
get_string_crc(const char *text)
{
    int index;
    int crc;

    CRC_START(crc);
    index = 0;
    while (text[index] != 0)
    {
        CRC_PASS(text[index], crc);
        index++;
    }
    CRC_END(crc);
    return crc;
}

/*****************************************************************************/
/* returns 0, event handled, 1 unhandled */
static int
rail_win_send_text(Window win)
{
    char *data = 0;
    struct stream *s;
    int len = 0;
    int flags;
    int crc;
    struct rail_window_data *rwd;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_win_send_text:");
    len = rail_win_get_text(win, &data);
    rwd = rail_get_window_data_safe(win);
    if (rwd != 0)
    {
        if (data != 0)
        {
            if (rwd->valid & RWD_TITLE)
            {
                crc = get_string_crc(data);
                if (rwd->title_crc == crc)
                {
                    LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_win_send_text: skipping, title not changed");
                    g_free(data);
                    XFree(rwd);
                    return 0;
                }
            }
        }
    }
    else
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "chansrv::rail_win_send_text: error rail_get_window_data_safe failed");
        g_free(data);
        return 1;
    }
    if (data && len > 0)
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_win_send_text: 0x%8.8lx text %s length %d",
                  win, data, len);
        make_stream(s);
        init_stream(s, len + 1024);
        flags = WINDOW_ORDER_TYPE_WINDOW | WINDOW_ORDER_FIELD_TITLE;
        out_uint32_le(s, 8); /* update title info */
        out_uint32_le(s, win); /* window id */
        out_uint32_le(s, flags); /* flags */
        out_uint32_le(s, len); /* title size */
        out_uint8a(s, data, len); /* title */
        s_mark_end(s);
        send_rail_drawing_orders(s->data, (int)(s->end - s->data));
        free_stream(s);
        /* update rail window data */
        rwd->valid |= RWD_TITLE;
        crc = get_string_crc(data);
        rwd->title_crc = crc;
        rail_set_window_data(win, rwd);
    }
    g_free(data);
    XFree(rwd);
    return 0;
}

/*****************************************************************************/
static int
rail_destroy_window(Window window_id)
{
    struct stream *s;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_destroy_window 0x%8.8lx", window_id);
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
static int
rail_show_window(Window window_id, int show_state)
{
    int flags;
    struct stream *s;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_show_window 0x%8.8lx 0x%x", window_id, show_state);
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
static int
rail_create_window(Window window_id, Window owner_id)
{
    int x;
    int y;
    tui32 width;
    tui32 height;
    tui32 border;
    Window root;
    tui32 depth;
    char *title_bytes = 0;
    int title_size = 0;
    XWindowAttributes attributes;
    int style;
    int ext_style;
    int num_window_rects = 1;
    int num_visibility_rects = 1;
    int i = 0;

    int flags;
    int index;
    int crc;
    Window transient_for = 0;
    struct rail_window_data *rwd;
    struct stream *s;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_create_window 0x%8.8lx", window_id);

    rwd = rail_get_window_data_safe(window_id);
    if (rwd == 0)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "chansrv::rail_create_window: error rail_get_window_data_safe failed");
        return 0;
    }
    XGetGeometry(g_display, window_id, &root, &x, &y, &width, &height,
                 &border, &depth);
    XGetWindowAttributes(g_display, window_id, &attributes);

    LOG_DEVEL(LOG_LEVEL_DEBUG, "  x %d y %d width %d height %d border_width %d", x, y, width,
              height, border);

    index = list_index_of(g_window_list, window_id);
    if (index == -1)
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "  create new window");
        flags = WINDOW_ORDER_TYPE_WINDOW | WINDOW_ORDER_STATE_NEW;
        list_add_item(g_window_list, window_id);
    }
    else
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "  update existing window");
        flags = WINDOW_ORDER_TYPE_WINDOW;
    }

    title_size = 0;
    title_bytes = 0;

    XGetTransientForHint(g_display, window_id, &transient_for);

    if (attributes.override_redirect)
    {
        style = RAIL_STYLE_TOOLTIP;
        ext_style = RAIL_EXT_STYLE_TOOLTIP;
        /* for tooltips, we don't grab the window text */
    }
    else if (transient_for > 0)
    {
        style = RAIL_STYLE_DIALOG;
        ext_style = RAIL_EXT_STYLE_DIALOG;
        owner_id = transient_for;
        title_size = rail_win_get_text(window_id, &title_bytes);
    }
    else
    {
        style = RAIL_STYLE_NORMAL;
        ext_style = RAIL_EXT_STYLE_NORMAL;
        title_size = rail_win_get_text(window_id, &title_bytes);
    }

    make_stream(s);
    init_stream(s, title_size + 1024 + num_window_rects * 8 + num_visibility_rects * 8);

    out_uint32_le(s, 2); /* create_window */
    out_uint32_le(s, window_id); /* window_id */
    out_uint32_le(s, owner_id); /* owner_window_id */
    flags |= WINDOW_ORDER_FIELD_OWNER;
    out_uint32_le(s, style); /* style */
    out_uint32_le(s, ext_style); /* extended_style */
    flags |= WINDOW_ORDER_FIELD_STYLE;
    out_uint32_le(s, 0x05); /* show_state */
    LOG_DEVEL(LOG_LEVEL_DEBUG, "  title %s", title_bytes);
    flags |= WINDOW_ORDER_FIELD_SHOW;
    if (title_size > 0)
    {
        out_uint16_le(s, title_size); /* title_size */
        out_uint8a(s, title_bytes, title_size); /* title */
        rwd->valid |= RWD_TITLE;
        crc = get_string_crc(title_bytes);
        rwd->title_crc = crc;
    }
    else
    {
        out_uint16_le(s, 5); /* title_size */
        out_uint8a(s, "title", 5); /* title */
        rwd->valid |= RWD_TITLE;
        rwd->title_crc = 0;
    }
    LOG_DEVEL(LOG_LEVEL_DEBUG, "  set title info %d", title_size);
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
    g_free(title_bytes);
    rail_set_window_data(window_id, rwd);
    XFree(rwd);
    return 0;
}

/*****************************************************************************/
/* returns 0, event handled, 1 unhandled */
int
rail_configure_request_window(XConfigureRequestEvent *config)
{
    int num_window_rects = 1;
    int num_visibility_rects = 1;
    int i = 0;
    int flags;
    int index;
    int window_id;
    int mask;
    int resized = 0;
    struct rail_window_data *rwd;

    struct stream *s;

    window_id = config->window;
    mask = config->value_mask;
    LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_configure_request_window: mask %d", mask);
    if (mask & CWStackMode)
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_configure_request_window: CWStackMode "
                  "detail 0x%8.8x above 0x%8.8lx", config->detail, config->above);
        if (config->detail == Above)
        {
            LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_configure_request_window: bring to front "
                      "window_id 0x%8.8x", window_id);
            /* 0x05 - Show the window in its current size and position. */
            rail_show_window(window_id, 5);
        }
    }
    rwd = rail_get_window_data(window_id);
    if (rwd == 0)
    {
        rwd = (struct rail_window_data *)g_malloc(sizeof(struct rail_window_data), 1);
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

    LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_configure_request_window: 0x%8.8x", window_id);


    LOG_DEVEL(LOG_LEVEL_DEBUG, "  x %d y %d width %d height %d border_width %d", config->x,
              config->y, config->width, config->height, config->border_width);

    index = list_index_of(g_window_list, window_id);
    if (index == -1)
    {
        /* window isn't mapped yet */
        LOG_DEVEL(LOG_LEVEL_ERROR, "chansrv::rail_configure_request_window: window not mapped");
        return 0;
    }

    flags = WINDOW_ORDER_TYPE_WINDOW;

    make_stream(s);
    init_stream(s, 1024 + num_window_rects * 8 + num_visibility_rects * 8);

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
int
rail_configure_window(XConfigureEvent *config)
{
    int num_window_rects = 1;
    int num_visibility_rects = 1;
    int i = 0;
    int flags;
    int index;
    int window_id;

    struct stream *s;

    window_id = config->window;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_configure_window 0x%8.8x", window_id);


    LOG_DEVEL(LOG_LEVEL_DEBUG, "  x %d y %d width %d height %d border_width %d", config->x,
              config->y, config->width, config->height, config->border_width);

    index = list_index_of(g_window_list, window_id);
    if (index == -1)
    {
        /* window isn't mapped yet */
        return 0;
    }

    flags = WINDOW_ORDER_TYPE_WINDOW;

    make_stream(s);
    init_stream(s, 1024 + num_window_rects * 8 + num_visibility_rects * 8);

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
static int
rail_desktop_resize(XEvent *lxevent)
{
    LOG_DEVEL(LOG_LEVEL_INFO, "rail_desktop_resize:");
    return 0;
}

/*****************************************************************************/
/* returns 0, event handled, 1 unhandled */
int
rail_xevent(void *xevent)
{
    XEvent *lxevent;
    XEvent lastevent;
    XWindowChanges xwc;
    int rv;
    int index;
    XWindowAttributes wnd_attributes;
    char *prop_name;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "chansrv::rail_xevent:");

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
            LOG_DEVEL(LOG_LEVEL_DEBUG, "  got PropertyNotify window_id 0x%8.8lx %s state new %d",
                      lxevent->xproperty.window, prop_name,
                      lxevent->xproperty.state == PropertyNewValue);

            if (list_index_of(g_window_list, lxevent->xproperty.window) < 0)
            {
                break;
            }

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
            LOG_DEVEL(LOG_LEVEL_DEBUG, "  got ConfigureRequest window_id 0x%8.8lx", lxevent->xconfigurerequest.window);
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
            LOG_DEVEL(LOG_LEVEL_DEBUG, " got CreateNotify window 0x%8.8lx parent 0x%8.8lx",
                      lxevent->xcreatewindow.window, lxevent->xcreatewindow.parent);
            rail_select_input(lxevent->xcreatewindow.window);
            break;

        case DestroyNotify:
            LOG_DEVEL(LOG_LEVEL_DEBUG, "  got DestroyNotify window 0x%8.8lx event 0x%8.8lx",
                      lxevent->xdestroywindow.window, lxevent->xdestroywindow.event);
            if (lxevent->xdestroywindow.window != lxevent->xdestroywindow.event)
            {
                break;
            }
            index = list_index_of(g_window_list, lxevent->xdestroywindow.window);
            if (index >= 0)
            {
                rail_destroy_window(lxevent->xdestroywindow.window);
                list_remove_item(g_window_list, index);
            }
            rv = 0;
            break;

        case MapRequest:
            LOG_DEVEL(LOG_LEVEL_DEBUG, "  got MapRequest window 0x%8.8lx", lxevent->xmaprequest.window);
            XMapWindow(g_display, lxevent->xmaprequest.window);
            break;

        case MapNotify:
            LOG_DEVEL(LOG_LEVEL_DEBUG, "  got MapNotify window 0x%8.8lx event 0x%8.8lx",
                      lxevent->xmap.window, lxevent->xmap.event);
            if (lxevent->xmap.window != lxevent->xmap.event)
            {
                break;
            }

            if (!is_window_valid_child_of_root(lxevent->xmap.window))
            {
                break;
            }

            XGetWindowAttributes(g_display, lxevent->xmap.window, &wnd_attributes);
            if (wnd_attributes.map_state == IsViewable)
            {
                rail_create_window(lxevent->xmap.window, g_root_window);
                if (!wnd_attributes.override_redirect)
                {
                    rail_win_set_state(lxevent->xmap.window, 0x1); /* NormalState */
                    rail_win_send_text(lxevent->xmap.window);
                }
                rv = 0;
            }
            break;

        case UnmapNotify:
            LOG_DEVEL(LOG_LEVEL_DEBUG, "  got UnmapNotify 0x%8.8lx", lxevent->xunmap.event);
            if (lxevent->xunmap.window != lxevent->xunmap.event)
            {
                break;
            }
            if (is_window_valid_child_of_root(lxevent->xunmap.window))
            {
                index = list_index_of(g_window_list, lxevent->xunmap.window);
                LOG_DEVEL(LOG_LEVEL_DEBUG, "  window 0x%8.8lx is unmapped", lxevent->xunmap.window);
                if (index >= 0)
                {
                    XGetWindowAttributes(g_display, lxevent->xunmap.window, &wnd_attributes);
                    if (wnd_attributes.override_redirect)
                    {
                        // remove popups
                        rail_destroy_window(lxevent->xunmap.window);
                        list_remove_item(g_window_list, index);
                    }
                    else
                    {
                        rail_show_window(lxevent->xunmap.window, 0x0);
                    }

                    rv = 0;
                }
            }
            break;

        case ConfigureNotify:
            LOG_DEVEL(LOG_LEVEL_DEBUG, "  got ConfigureNotify 0x%8.8lx event 0x%8.8lx", lxevent->xconfigure.window,
                      lxevent->xconfigure.event);
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
            LOG_DEVEL(LOG_LEVEL_DEBUG, "  got FocusIn");
            g_focus_win = lxevent->xfocus.window;
            break;

        case FocusOut:
            LOG_DEVEL(LOG_LEVEL_DEBUG, "  got FocusOut");
            break;

        case ButtonPress:
            LOG_DEVEL(LOG_LEVEL_DEBUG, "  got ButtonPress");
            break;

        case EnterNotify:
            LOG_DEVEL(LOG_LEVEL_DEBUG, "  got EnterNotify");
            break;

        case LeaveNotify:
            LOG_DEVEL(LOG_LEVEL_DEBUG, "  got LeaveNotify");
            break;

        case ReparentNotify:
            LOG_DEVEL(LOG_LEVEL_DEBUG, "  got ReparentNotify window 0x%8.8lx parent 0x%8.8lx "
                      "event 0x%8.8lx x %d y %d override redirect %d",
                      lxevent->xreparent.window, lxevent->xreparent.parent,
                      lxevent->xreparent.event, lxevent->xreparent.x,
                      lxevent->xreparent.y, lxevent->xreparent.override_redirect);

            if (lxevent->xreparent.window != lxevent->xreparent.event)
            {
                break;
            }
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

        default:
            if (g_xrr_event_base > 0)
            {
                if (lxevent->type == g_xrr_event_base + RRScreenChangeNotify)
                {
                    rail_desktop_resize(lxevent);
                    rv = 0;
                    break;
                }
            }
    }

    return rv;
}
