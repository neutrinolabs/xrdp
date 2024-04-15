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
#include <unistd.h>

#include "scancode.h"
#include "xrdp_constants.h"

// cppcheck doesn't always set this macro to something in double-quotes
#if defined(__cppcheck__)
#undef PACKAGE_VERSION
#endif

#if !defined(PACKAGE_VERSION)
#define PACKAGE_VERSION "???"
#endif

#define NUM_STATES 9
#define STATE_NUMLOCK 8

// Scancodes affected by numlock
#define IS_KEYPAD_SCANCODE(s) \
    ((s) >= XR_RDP_SCAN_MIN_NUMLOCK && (s) <= XR_RDP_SCAN_MAX_NUMLOCK)

#define MAX_COMMENTS 10

static void
usage(const char *argv0, int status)
{
    fprintf(stderr, "Usage: %s [-c comment] [-c comment...] out_filename\n",
            argv0);
    fprintf(stderr, "Example: %s -c \"en-US pc104 keyboard\""
            " /etc/xrdp/km-00000409.ini\n", argv0);
    exit(status);
}

int main(int argc, char **argv)
{
    int opt;
    char text[256];
    char *displayname = NULL;
    char *outfname;
    const char *sections[NUM_STATES] =
    {
        "noshift", "shift", "altgr", "shiftaltgr",
        "capslock", "capslockaltgr", "shiftcapslock", "shiftcapslockaltgr",
        "numlock"
    };
    int states[NUM_STATES] = {0, 1, 0x80, 0x81, 2, 0x82, 3, 0x83, 0x10};
    int idx;
    int char_count;
    int nbytes = 0;
    int unicode;
    Display *dpy;
    KeySym ks;
    const char *ksstr;
    FILE *outf;
    XKeyPressedEvent e = {0};
    wchar_t wtext[256];
    const char *comment[MAX_COMMENTS] = {0};
    int comment_count = 0;

    setlocale(LC_CTYPE, "");

    while ((opt = getopt(argc, argv, "c:")) != -1)
    {
        switch (opt)
        {
            case 'c':
                if (comment_count < MAX_COMMENTS)
                {
                    comment[comment_count++] = optarg;
                }
                break;
            default: /* '?' */
                usage(argv[0], 1);
        }
    }

    if ((optind + 1) != argc)
    {
        usage(argv[0], 1);
    }

    outfname = argv[optind];
    dpy = XOpenDisplay(displayname);

    if (!dpy)
    {
        fprintf(stderr, "%s:  unable to open display '%s'\n",
                argv[0], XDisplayName(displayname));
        return 1;
    }

    outf = fopen(outfname, "w");

    if (outf == NULL)
    {
        fprintf(stderr, "%s:  unable to create file '%s'\n", argv[0], outfname);
        XCloseDisplay(dpy);
        return 1;
    }

    fprintf(outf, "# Created by %s version " PACKAGE_VERSION "\n", argv[0]);
    for (idx = 0; idx < comment_count; ++idx)
    {
        fprintf(outf, "# %s\n", comment[idx]);
    }
    fprintf(outf, "[General]\nversion=2\n");

    e.type = KeyPress;
    e.serial = 16;
    e.send_event = True;
    e.display = dpy;
    e.same_screen = True;

    for (idx = 0; idx < NUM_STATES; idx++) /* Sections and states */
    {
        unsigned short scancode;
        unsigned int iter;
        fprintf(outf, "\n[%s]\n", sections[idx]);
        e.state = states[idx];

        iter = 0;
        while ((scancode = scancode_get_next(&iter)) != 0)
        {
            // Numlock state table can be very small
            if (idx == STATE_NUMLOCK && !IS_KEYPAD_SCANCODE(scancode))
            {
                continue;
            }

            e.keycode = scancode_to_keycode(scancode);
            nbytes = XLookupString(&e, text, 255, &ks, NULL);
            if (ks == NoSymbol)
            {
                continue;
            }
            ksstr = XKeysymToString(ks);
            if (ksstr == NULL)
            {
                continue;
            }
            text[nbytes] = 0;
            char_count = mbstowcs(wtext, text, 255);
            unicode = 0;

            if (char_count == 1)
            {
                unicode = wtext[0];
            }

            if (scancode > 0xff)
            {
                fputs("E0_", outf);
            }
            fprintf(outf, "%02X=\"%d", (scancode & 0xff), (int)ks);
            if (unicode != 0)
            {
                fprintf(outf, ":U+%04X", unicode);
            }
            fprintf(outf, "\"  # %s\n", ksstr);
        }
    }

    XCloseDisplay(dpy);
    fclose(outf);
    return 0;
}
