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

 cs czech 0x405
 de german 0x407
 en-us us english 0x409
 fr french 0x40c
 it italy 0x410
 br Portuguese (Brazil) 0x416
 ru russian 0x419
 se swedish 0x41d
 en-uk uk english 0x809
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <locale.h>

int main(int argc, char **argv)
{
  const char* programname;
  char text[256];
  char* displayname = NULL;
  char* outfname;
  char* sections[5] = {"noshift", "shift", "altgr", "capslock", "shiftcapslock"};
  int states[5] = {0, 1, 0x80, 2, 3};
  int i;
  int idx;
  int char_count;
  int nbytes = 0;
  int unicode;
  Display* dpy;
  KeySym ks;
  FILE* outf;
  XKeyPressedEvent e;
  wchar_t wtext[256];

  setlocale(LC_CTYPE, "");
  programname = argv[0];
  if (argc != 2)
  {
    fprintf(stderr, "Usage: %s out_filename\n", programname);
    fprintf(stderr, "Example: %s /etc/xrdp/km-0409.ini\n", programname);
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
  for (idx = 0; idx < 5; idx++) /* Sections and states */
  {
    fprintf(outf, "[%s]\n", sections[idx]);
    e.state = states[idx];
    for (i = 8; i <= 137; i++) /* Keycodes */
    {
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
    if (idx != 4)
    {
      fprintf(outf, "\n");
    }
  }
  XCloseDisplay(dpy);
  fclose(outf);
  return 0;
}
