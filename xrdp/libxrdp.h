/*
   xrdp: A Remote Desktop Protocol server.
   Copyright (C) Jay Sorg 2004-2005
   use freely
*/

int server_init(void);
int server_exit(void);
int server_connect(int sck, int* width, int* height, int* bpp);
int server_loop(int sck);
int server_set_callback(int (* callback)(int, int, int));
int server_begin_update(void);
int server_end_update(void);
int server_fill_rect(int x, int y, int cx, int cy, int color);
int server_screen_blt(int x, int y, int cx, int cy, int srcx, int srcy);
int server_paint_rect(int x, int y, int cx, int cy, char* data);
int server_set_cursor(int x, int y, char* data, char* mask);
int server_palette(int* palette);

