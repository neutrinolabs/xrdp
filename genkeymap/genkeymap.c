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
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.

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
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <locale.h>

const char *ProgramName;
Display *dpy;

int main(int argc, char **argv) {
	char *displayname = NULL;
	char *outfname;
	KeySym ks;
	int i, idx;
	FILE *outf;	
	char *ksname = NULL;
	XKeyPressedEvent e;
	char text[256];
	wchar_t wtext[256];
	int char_count;
	int nbytes = 0;
	int unicode;

	setlocale(LC_CTYPE, "");

	/* --------------- */
	
	char *sections[5] = {"noshift", "shift", "altgr", "capslock", "shiftcapslock"};
	int states[5] = {0, 1, 0x80, 2, 3};
	
	/* --------------- */
	
	ProgramName = argv[0];	
	
	if (argc != 2) {		
		fprintf(stderr, "Usage: %s out_filename\n", ProgramName);
		exit(1);
	}
	outfname = argv[1];
	
	
	dpy = XOpenDisplay (displayname);
	if (!dpy) {	
		fprintf(stderr, "%s:  unable to open display '%s'\n",
				ProgramName, XDisplayName (displayname));
		exit(1);
	}
	
	/* --------------- */
	
	e.type = KeyPress;
	e.serial = 16;
	e.send_event = True;
	e.display = dpy;	
	e.window = (Window) NULL;
	e.root = (Window) NULL;
	e.time = 0;
	e.x = e.y = 0;
	e.x_root = e.y_root = 0;
	e.same_screen = True;
	
	outf = fopen(outfname, "w");
	
	for (idx=0; idx<5; idx++) { // Sections and states
		fprintf(outf, "[%s]\n", sections[idx]);
		e.state = states[idx];
		for (i=8; i<=137; i++) { // Keycodes
			e.keycode = i;			
			nbytes = XLookupString (&e, text, 255, &ks, NULL);
			
			if (ks == NoSymbol)
				ksname = "NoSymbol";
			else if (!(ksname = XKeysymToString (ks)))
				ksname = "(no name)";
			char_count = mbstowcs(wtext, text, 255);
			unicode = 0;
			if (char_count == 1)
			{
				unicode = wtext[0];
			}
			//printf("%s %d %x\n", text, char_count, wtext[0]);
			fprintf(outf, "Key%d=%d:%d\n", i, (int) ks, unicode);
		}
		if (idx!=4) fprintf(outf, "\n");
	}
	
	XCloseDisplay (dpy);
	fclose(outf);	
	
	return (0);
}
