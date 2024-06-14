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
#include <X11/extensions/XKBrules.h>
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

#define KEYMAP_FILE_FORMAT_VERSION "2"

// Scancodes affected by numlock
#define IS_KEYPAD_SCANCODE(s) \
    ((s) >= XR_RDP_SCAN_MIN_NUMLOCK && (s) <= XR_RDP_SCAN_MAX_NUMLOCK)

#define MAX_COMMENTS 10

/**
 * Contains info about the current keyboard setting
 */
struct kbd_info
{
    char *keycode_set; ///< See 'setxkbmap -v'
    char *rules; ///< See 'setxkbmap -query'
    char *model; ///< See 'setxkbmap -query'
    char *layout; ///< See 'setxkbmap -query'
    char *variant; ///< See 'setxkbmap -query'
    char *options; ///< See 'setxkbmap -query' (comma separated)
};

/*****************************************************************************/
/**
 * Print brief info about program usage and exit
 * @param programname Unqualified name of program
 * @param status Exit status
 */
static void
usage(const char *programname, int status)
{
    fprintf(stderr, "Usage: %s [ -k keycode_set] [-c comment] [-c comment...]"
            " out_filename\n", programname);
    fprintf(stderr, "Example: %s -r evdev -c \"en-US pc104 keyboard\""
            " /etc/xrdp/km-00000409.toml\n", programname);
    exit(status);
}

/*****************************************************************************/
/**
 * Free a kbd_info struct
 * @param kbd_info struct to free. May be incomplete or NULL
 */
static void
free_kbd_info(struct kbd_info *kbd_info)
{
    if (kbd_info != NULL)
    {
        free(kbd_info->keycode_set);
        free(kbd_info->rules);
        free(kbd_info->model);
        free(kbd_info->layout);
        free(kbd_info->variant);
        free(kbd_info->options);
        free(kbd_info);
    }
}

/*****************************************************************************/
/**
 * Queries the X server to get information about the current keyboard
 * @param dpy X11 Display
 * @param programname Unqualified name of program
 * @return kbd_info struct, or NULL
 *
 * The structure may be incomplete if some data could not be obtained
 */
static struct kbd_info *
get_kbd_info(Display *dpy, const char *programname)
{
    struct kbd_info *kbd_info;
    char *rules;
    XkbRF_VarDefsRec vd;
    XkbDescPtr kbdesc = NULL;

    if ((kbd_info = (struct kbd_info *)malloc( sizeof(*kbd_info))) == NULL)
    {
        fprintf(stderr, "%s: Out of memory\n", programname);
    }
    else if ((kbdesc = XkbAllocKeyboard()) == NULL)
    {
        fprintf(stderr, "%s:  unable to allocate keyboard desc\n",
                programname);
        free_kbd_info(kbd_info);
        kbd_info = NULL;
    }
    else if (XkbGetNames(dpy, XkbKeycodesNameMask, kbdesc) != Success)
    {
        fprintf(stderr, "%s:  unable to obtain keycode name for keyboard\n",
                programname);
        free_kbd_info(kbd_info);
        kbd_info = NULL;
    }
    else
    {
        char *symatom = XGetAtomName(dpy, kbdesc->names->keycodes);
        kbd_info->keycode_set = strdup(symatom);
        XFree(symatom);

        if (XkbRF_GetNamesProp(dpy, &rules, &vd) == 0 || rules == NULL)
        {
            fprintf(stderr, "%s: Couldn't interpret %s property\n",
                    programname, _XKB_RF_NAMES_PROP_ATOM);
            kbd_info->rules = NULL;
            kbd_info->model = NULL;
            kbd_info->layout = NULL;
            kbd_info->variant = NULL;
            kbd_info->options = NULL;
        }
        else
        {
            kbd_info->rules = rules;
            kbd_info->model = vd.model;
            kbd_info->layout = vd.layout;
            kbd_info->variant = vd.variant;
            kbd_info->options = vd.options;
        }
    }

    if (kbdesc != NULL)
    {
        XkbFreeKeyboard(kbdesc, 0, True);
    }

    return kbd_info;
}

/*****************************************************************************/
/**
 * Outputs a comment containing the last setxkbmap command
 *
 * @param outf Output file
 * @param kbd_info Keyboard info struct
 *
 */
static void
output_setxkbmap_comment(FILE *outf, const struct kbd_info *kbd_info)
{
    if (kbd_info->model != NULL || kbd_info->layout != NULL ||
            kbd_info->variant != NULL || kbd_info->options != NULL)
    {
        fprintf(outf, "# setxkbmap -rules %s", kbd_info->rules);
        if (kbd_info->model != NULL)
        {
            fprintf(outf, " -model %s", kbd_info->model);
        }
        if (kbd_info->layout != NULL)
        {
            fprintf(outf, " -layout %s", kbd_info->layout);
        }
        if (kbd_info->variant != NULL)
        {
            fprintf(outf, " -variant %s", kbd_info->variant);
        }
        if (kbd_info->options != NULL)
        {
            // Options is comma-separated, but to achieve the same effect
            // with the command we need to use multiple '-option' args
            char *optionstr = strdup(kbd_info->options);
            if (optionstr != NULL)
            {
                char *p = strtok(optionstr, ",");
                fprintf(outf, " -option \"\"");
                while (p != NULL)
                {
                    fprintf(outf, " -option %s", p);
                    p = strtok(NULL, ",");
                }
                free(optionstr);
            }
        }
        putc('\n', outf);
    }
}

/*****************************************************************************/
/**
 * Output a section of the keymap file
 * @param outf Output file
 * @param dpy X display
 * @param section name Section name (e.g. 'shift')
 * @param event_state Modifier state needed for XKeyPressedEvent
 */
static void
output_file_section(FILE *outf,
                    Display *dpy,
                    const char *section_name,
                    unsigned int event_state)
{
    XKeyPressedEvent e =
    {
        .type = KeyPress,
        .serial = 16,
        .send_event = True,
        .display = dpy,
        .state = event_state,
        .same_screen = True
    };
    int char_count;
    int nbytes = 0;
    int unicode;
    KeySym ks;
    const char *ksstr;
    unsigned short scancode;
    unsigned int iter = 0;
    char text[256];
    wchar_t wtext[256];

    int is_numlock_section = (strcmp(section_name, "numlock") == 0);

    fprintf(outf, "\n[%s]\n", section_name);

    while ((scancode = scancode_get_next(&iter)) != 0)
    {
        // Numlock state table can be very small
        if (is_numlock_section && !IS_KEYPAD_SCANCODE(scancode))
        {
            continue;
        }

        e.keycode = scancode_to_x11_keycode(scancode);
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


/*****************************************************************************/
/**
 * Main
 * @param argc Argument count
 * @param argv Pointers to arguments
 * @return 0 for success
 */
int main(int argc, char **argv)
{
    const char *programname;
    int opt;
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
    Display *dpy = NULL;
    FILE *outf = NULL;
    const char *comment[MAX_COMMENTS] = {0};
    int comment_count = 0;
    const char *keycode_set = NULL;
    struct kbd_info *kbd_info = NULL;
    int status = 1;

    setlocale(LC_CTYPE, "");
    if (strrchr(argv[0], '/') != NULL)
    {
        programname = strrchr(argv[0], '/') + 1;
    }
    else
    {
        programname = argv[0];
    }

    while ((opt = getopt(argc, argv, "c:k:")) != -1)
    {
        switch (opt)
        {
            case 'c':
                if (comment_count < MAX_COMMENTS)
                {
                    comment[comment_count++] = optarg;
                }
                break;

            case 'k':
                keycode_set = optarg;
                break;

            default: /* '?' */
                usage(programname, 1);
        }
    }

    if ((optind + 1) != argc)
    {
        usage(programname, 1);
    }

    outfname = argv[optind];

    dpy = XOpenDisplay(displayname);
    if (!dpy)
    {
        fprintf(stderr, "%s:  unable to open display '%s'\n",
                programname, XDisplayName(displayname));
        goto finish;
    }

    if ((kbd_info = get_kbd_info(dpy, programname)) == 0)
    {
        // An error has already been logged
        goto finish;
    }

    // If the keycode set isn't specified, use the one returned
    // by the XKB extension
    if (keycode_set == NULL)
    {
        keycode_set = kbd_info->keycode_set;
    }

    if (scancode_set_keycode_set(keycode_set) != 0)
    {
        fprintf(stderr, "%s: keycode set '%s' is not recognised\n",
                programname, keycode_set);
        goto finish;
    }

    if ((outf = fopen(outfname, "w")) == NULL)
    {
        fprintf(stderr, "%s:  unable to create file '%s'\n",
                programname, outfname);
        goto finish;
    }

    fprintf(outf, "# Created by %s V" PACKAGE_VERSION
            "\n# Key code set: %s\n",
            programname, keycode_set);

    output_setxkbmap_comment(outf, kbd_info);

    for (idx = 0; idx < comment_count; ++idx)
    {
        fprintf(outf, "# %s\n", comment[idx]);
    }

    fprintf(outf, "\n[General]\nversion=" KEYMAP_FILE_FORMAT_VERSION "\n");

    for (idx = 0; idx < NUM_STATES; idx++) /* Sections and states */
    {
        output_file_section(outf, dpy, sections[idx], states[idx]);
    }

    status = 0; // Successful run

finish:
    free_kbd_info(kbd_info);
    if (dpy != NULL)
    {
        XCloseDisplay(dpy);
    }
    if (outf != NULL)
    {
        fclose(outf);
    }
    return status;
}
