/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * genkeymap.c
 * Copyright (C) Ádám Wallner 2008 <wallner@bitbaro.hu>
 *
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * main.cc is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with main.cc.  If not, write to:
 * The Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor
 * Boston, MA  02110-1301, USA

  Updated Jay Sorg 2009

 cs Czech 0x405
 de German 0x407
 en-us US English 0x409
 fr French 0x40c
 it Italian 0x410
 br Portuguese (Brazil) 0x416
 ru Russian 0x419
 se Swedish 0x41d
 en-uk UK English 0x809
*/

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <locale.h>

extern int xfree86_to_evdev[137-8];

int main(int argc, char **argv)
{
    const char *programname;
    char text[256];
    char *displayname = NULL;
    char *outfname;
    const char *sections[8] = {
        "noshift", "shift", "altgr", "shiftaltgr",
        "capslock", "capslockaltgr", "shiftcapslock", "shiftcapslockaltgr"
    };
    int states[8] = {0, 1, 0x80, 0x81, 2, 0x82, 3, 0x83};
    int i;
    int idx;
    int char_count;
    int nbytes = 0;
    int unicode;
    Display *dpy;
    KeySym ks;
    FILE *outf;
    XKeyPressedEvent e;
    wchar_t wtext[256];
    XkbDescPtr kbdesc;
    char *symatom;
    int is_evdev;

    setlocale(LC_CTYPE, "");
    programname = argv[0];

    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s out_filename\n", programname);
        fprintf(stderr, "Example: %s /etc/xrdp/km-00000409.ini\n", programname);
        return 1;
    }

    outfname = argv[1];
    dpy = XOpenDisplay(displayname);

    if (!dpy)
    {
        fprintf(stderr, "%s:  unable to open display '%s'\n",
                programname, XDisplayName(displayname));
        return 1;
    }

    /* check whether evdev is used */
    kbdesc = XkbAllocKeyboard();
    if (!kbdesc)
    {
        fprintf(stderr, "%s:  unable to allocate keyboard desc\n",
                programname);
        XCloseDisplay(dpy);
        return 1;
    }

    if (XkbGetNames(dpy, XkbKeycodesNameMask, kbdesc) != Success)
    {
        fprintf(stderr, "%s:  unable to obtain keycode name for keyboard\n",
                programname);
        XkbFreeKeyboard(kbdesc, 0, True);
        XCloseDisplay(dpy);
        return 1;
    }

    symatom = XGetAtomName(dpy, kbdesc->names->keycodes);
    is_evdev = !strncmp(symatom, "evdev", 5);
    XFree(symatom);
    XkbFreeKeyboard(kbdesc, 0, True);

    outf = fopen(outfname, "w");

    if (outf == NULL)
    {
        fprintf(stderr, "%s:  unable to create file '%s'\n", programname, outfname);
        XCloseDisplay(dpy);
        return 1;
    }

    memset(&e, 0, sizeof(e));
    e.type = KeyPress;
    e.serial = 16;
    e.send_event = True;
    e.display = dpy;
    e.same_screen = True;

    for (idx = 0; idx < 8; idx++) /* Sections and states */
    {
        fprintf(outf, "[%s]\n", sections[idx]);
        e.state = states[idx];

        for (i = 8; i <= 137; i++) /* Keycodes */
        {
            if (is_evdev)
                e.keycode = xfree86_to_evdev[i-8];
            else
                e.keycode = i;
            nbytes = XLookupString(&e, text, 255, &ks, NULL);
            text[nbytes] = 0;
            char_count = mbstowcs(wtext, text, 255);
            unicode = 0;

            if (char_count == 1)
            {
                unicode = wtext[0];
            }

            fprintf(outf, "Key%d=%d:%d\n", i, (int) ks, unicode);
        }

        if (idx != 7)
        {
            fprintf(outf, "\n");
        }
    }

    XCloseDisplay(dpy);
    fclose(outf);
    return 0;
}
