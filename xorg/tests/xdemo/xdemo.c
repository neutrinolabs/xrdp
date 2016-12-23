/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Laxmikant Rashinkar 2012
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "common.h"

// LK_TODO
// http://tronche.com/gui/x/xlib/GC/convenience-functions/fill-tile-and-stipple.html
// fill stipple

// drawfonts: XDrawString, XDrawImageString XDrawText XLoadFont XTextExtents
// http://www.ac3.edu.au/SGI_Developer/books/XLib_PG/sgi_html/apa.html
// http://www.ac3.edu.au/SGI_Developer/books/XLib_PG/sgi_html/index.html

// use jpg lib to convert bmp to jpg and vice versa

#define MAX_COLORS          5
#define SCROLL_JUMP         1   // scroll in increments of g_winHeight
#define SCROLL_SMOOTH1      2   // scroll using XPutImage + XCopyArea
#define SCROLL_SMOOTH2      3   // scroll using XPutImage only
#define SCROLL_SMOOTH3      4   // scroll using XPutImage only
#define SCROLL_SMOOTH4      5   // scroll using XPutImage only

int parse_bmp(char *filename, struct pic_info *);
int drawBMP(char *filename, int scroll_type);
int signal_tcp_proxy(char *proxy_app);

// globals
Display     *g_disp;
Window      g_win;
XColor      g_colors[MAX_COLORS];
GC          g_gc;
int         g_winWidth;
int         g_winHeight;
int         g_delay_dur;

void start_timer(struct timeval *tv)
{
    gettimeofday(tv, NULL);
}


uint32_t time_elapsed_ms(struct timeval tv)
{
    struct timeval  tv_now;
    uint32_t        dur;

    gettimeofday(&tv_now, NULL);
    dur = ((tv_now.tv_sec - tv.tv_sec) * 1000) + ((tv_now.tv_usec - tv.tv_usec) / 1000);
    return dur;
}


uint32_t time_elapsed_us(struct timeval tv)
{
    struct timeval  tv_now;
    uint32_t        dur;

    gettimeofday(&tv_now, NULL);
    dur = ((tv_now.tv_sec - tv.tv_sec) * 1000000) + (tv_now.tv_usec - tv.tv_usec);
    return dur;
}

int drawLines(int count)
{
    int x1;
    int y1;
    int x2;
    int y2;
    int i;
    int index;

    if (count <= 0)
    {
        return 0;                  // nothing to do
    }

    srandom(time(NULL));
    XClearArea(g_disp, g_win, 0, 0, g_winWidth, g_winHeight, 0);

    for (i = 0, index = 0; i < count; i++)
    {
        x1 = random() % g_winWidth;
        y1 = random() % g_winHeight;
        x2 = random() % g_winWidth;
        y2 = random() % g_winHeight;
        XSetForeground(g_disp, g_gc, g_colors[index++].pixel);

        if (index == MAX_COLORS)
        {
            index = 0;
        }

        // from-to
        XDrawLine(g_disp, g_win, g_gc, x1, y1, x2, y2);
        XFlush(g_disp);
        usleep(g_delay_dur);
    }

    return 0;
}

// LK_TODO support user defined w and h

int drawRectangles(int count)
{
    int x1;
    int y1;
    int w;
    int h;
    int i;
    int index;

    if (count <= 0)
    {
        return 0;  // nothing to do
    }

    srandom(time(NULL));
    XClearArea(g_disp, g_win, 0, 0, g_winWidth, g_winHeight, 0);

    for (i = 0, index = 0; i < count; i++)
    {
        x1 = random() % g_winWidth;
        y1 = random() % g_winHeight;
        w = 160;
        h = 140;
        XSetForeground(g_disp, g_gc, g_colors[index++].pixel);

        if (index == MAX_COLORS)
        {
            index = 0;
        }

        //XDrawRectangle(g_disp, g_win, g_gc, x1, y1, w, h);
        XFillRectangle(g_disp, g_win, g_gc, x1, y1, w, h);
        XFlush(g_disp);
        usleep(g_delay_dur);
    }

    return 0;
}

int drawFont(int count, char *msg)
{
    int  x1;
    int  y1;
    int  i;
    int  index;

#ifdef CHANGE_FONT_SIZE
    int  w;
    int  h;
    int  actual_count;
    char **font_list;
#endif

    if (count <= 0)
    {
        return 0; // nothing to do
    }

    srandom(time(NULL));
    XClearArea(g_disp, g_win, 0, 0, g_winWidth, g_winHeight, 0);

#ifdef CHANGE_FONT_SIZE
    font_list = XListFonts(g_disp, "−*−courier−*−*−*−*−0−0−*−*−*−0−*−*", 2000, &actual_count);

    if (!font_list)
    {
        printf("actual_count=%d\n", actual_count);

        for (i = 0; i < actual_count; i++)
        {
            printf("%s\n", font_list[i]);
        }

        XFreeFontNames(font_list);
    }
    else
    {
        printf("XListFonts() returned NULL\n");
    }

#endif

    srandom(time(NULL));

    for (i = 0, index = 0; i < count; i++)
    {
        x1 = random() % g_winWidth;
        y1 = random() % g_winHeight;
        XSetForeground(g_disp, g_gc, g_colors[index++].pixel);

        if (index == MAX_COLORS)
        {
            index = 0;
        }

        XDrawString(g_disp, g_win, g_gc, x1, y1, msg, strlen(msg));
        XFlush(g_disp);
        usleep(g_delay_dur);
    }

    return 0; // nothing to do
}

static void
drawoffscreen(void)
{
    int depth;
    Pixmap pixmap1;
    Pixmap pixmap2;

    printf("draw off screen, should see green rect\n");
    depth = DefaultDepth(g_disp, DefaultScreen(g_disp));

    /* blue */
    pixmap1 = XCreatePixmap(g_disp, g_win, 64, 64, depth);
    XSetForeground(g_disp, g_gc, 0x000000ff);
    XFillRectangle(g_disp, pixmap1, g_gc, 0, 0, 64, 64);

    /* green */
    pixmap2 = XCreatePixmap(g_disp, g_win, 64, 64, depth);
    XSetForeground(g_disp, g_gc, 0x0000ff00);
    XFillRectangle(g_disp, pixmap2, g_gc, 0, 0, 64, 64);

    /* copy green to blue */
    XCopyArea(g_disp, pixmap2, pixmap1, g_gc, 0, 0, 64, 64, 0, 0);

    /* put on screen */
    XCopyArea(g_disp, pixmap1, g_win, g_gc, 0, 0, 64, 64, 128, 128);

    XFreePixmap(g_disp, pixmap1);
    XFreePixmap(g_disp, pixmap2);
}

/**
 * display a usage message
 */
static void
usage(void)
{
    printf("usage: xdemo [-l] [-r] [-s] [-f <string>] [-i <image file>] [-g <WxH>] [-c <count>] [-o <scroll type>] [-d <delay>] -z\n");
    printf("         -l                        draw lines\n");
    printf("         -r                        draw fill rectangles\n");
    printf("         -s                        draw stipple rectangles\n");
    printf("         -f <string>               draw string using fonts\n");
    printf("         -i <image file>           draw image\n");
    printf("         -g <WxH>                  geometry, default is 640x480\n");
    printf("         -c <count>                iteration count, default is 5000\n");
    printf("         -d <delay>                loop delay in micro seconds, default 1000\n");
    printf("         -o <jump|smooth1|smooth2| define scrolling method\n");
    printf("             smooth3|smooth4>\n");
    printf("         -z <proxy_app>            zero proxy counters for specified application\n");
    printf("         -j                        offscreen to offscreen test\n\n");
}

int
main(int argc, char **argv)
{
    XEvent          evt;
    Colormap        colormap;
    struct timeval  tv;
    int             screenNumber;
    long            eventMask;
    unsigned long   white;
    unsigned long   black;
    Status          rc;
    int             iters;
    int             opt;
    int             draw_lines;
    int             draw_rects;
    int             draw_stipples;
    int             draw_fonts;
    int             draw_image;
    int             draw_offscreen;
    int             zero_counters;
    int             scroll_type;
    char            image_file[256];
    char            proxy_app[256];
    char            msg[4096];

    // set some defaults
    g_winWidth = 640;
    g_winHeight = 480;
    iters = 5000;
    draw_lines = 1;
    draw_rects = 1;
    draw_stipples = 1;
    draw_fonts = 1;
    draw_image = 1;
    draw_offscreen = 1;
    g_delay_dur = 1000;
    scroll_type = SCROLL_SMOOTH1;
    zero_counters = 0;
    strcpy(image_file, "yosemite.bmp");
    strcpy(msg, "To be or not to be!");

    // process cmd line args
    opterr = 0;

    while ((opt = getopt(argc, argv, "lrsjg:c:f:i:d:o:z:")) != -1)
    {
        switch (opt)
        {

            case 'j':
                draw_lines = 0;
                draw_rects = 0;
                draw_stipples = 0;
                draw_fonts = 0;
                draw_image = 0;
                draw_offscreen = 1;
                break;

            case 'g':

                if (sscanf(optarg, "%dx%d", &g_winWidth, &g_winHeight) != 2)
                {
                    fprintf(stderr, "\nerror: invalid geometry specified\n\n");
                    usage();
                    return -1;
                }

                break;

            case 'c':

                if (sscanf(optarg, "%d", &iters) != 1)
                {
                    fprintf(stderr, "\nerror: invalid count specified\n\n");
                    usage();
                    return -1;
                }

                break;

            case 'l':
                draw_lines = 1;
                draw_rects = 0;
                draw_stipples = 0;
                draw_fonts = 0;
                draw_image = 0;
                draw_offscreen = 0;
                break;

            case 'r':
                draw_rects = 1;
                draw_lines = 0;
                draw_stipples = 0;
                draw_fonts = 0;
                draw_image = 0;
                draw_offscreen = 0;
                break;

            case 's':
                draw_stipples = 1;
                draw_lines = 0;
                draw_rects = 0;
                draw_fonts = 0;
                draw_image = 0;
                draw_offscreen = 0;
                break;

            case 'f':

                if (strlen(optarg) <= 0)
                {
                    fprintf(stderr, "\nerror: -f option requires an argument\n\n");
                    usage();
                    return -1;
                }

                draw_fonts = 1;
                strncpy(msg, optarg, 4096);
                draw_lines = 0;
                draw_rects = 0;
                draw_stipples = 0;
                draw_image = 0;
                draw_offscreen = 0;
                break;

            case 'i':

                if (strlen(optarg) <= 0)
                {
                    fprintf(stderr, "\nerror: -i option requires an argument\n\n");
                    usage();
                    return -1;
                }

                draw_image = 1;
                strncpy(image_file, optarg, 255);
                draw_lines = 0;
                draw_rects = 0;
                draw_stipples = 0;
                draw_fonts = 0;
                draw_offscreen = 0;
                break;

            case 'h':
                usage();
                return 0;
                break;

            case 'v':
                printf("xdemo Ver 1.0\n");
                return 0;
                break;

            case 'd':

                if (sscanf(optarg, "%d", &g_delay_dur) != 1)
                {
                    fprintf(stderr, "\nerror: -d option requires an argument\n\n");
                    usage();
                    return -1;
                }

                break;

            case 'z':

                if (strlen(optarg) <= 0)
                {
                    fprintf(stderr, "\nerror: invalid proxy application specified\n\n");
                    usage();
                    return -1;
                }

                strcpy(proxy_app, optarg);
                printf("##### LK_TODO: proxy_app=%s\n", proxy_app);
                zero_counters = 1;
                break;

            case 'o':

                if (strcmp(optarg, "jump") == 0)
                {
                    scroll_type = SCROLL_JUMP;
                }
                else if (strcmp(optarg, "smooth1") == 0)
                {
                    scroll_type = SCROLL_SMOOTH1;
                }
                else if (strcmp(optarg, "smooth2") == 0)
                {
                    scroll_type = SCROLL_SMOOTH2;
                }
                else if (strcmp(optarg, "smooth3") == 0)
                {
                    scroll_type = SCROLL_SMOOTH3;
                }
                else if (strcmp(optarg, "smooth4") == 0)
                {
                    scroll_type = SCROLL_SMOOTH4;
                }
                else
                {
                    fprintf(stderr, "\ninvalid scroll type specified\n\n");
                    usage();
                    return -1;
                }

                break;

            default:
                usage();
                return -1;
        }
    }

    // must have at least one operation
    if ((!draw_lines) && (!draw_rects) && (!draw_stipples) &&
            (!draw_fonts) && (!draw_image) && (!draw_offscreen))
    {
        usage();
        return -1;
    }

    g_disp = XOpenDisplay(NULL);

    if (!g_disp)
    {
        dprint("error opening X display\n");
        exit(-1);
    }

    screenNumber = DefaultScreen(g_disp);
    white = WhitePixel(g_disp, screenNumber);
    black = BlackPixel(g_disp, screenNumber);

    g_win = XCreateSimpleWindow(g_disp,
                                DefaultRootWindow(g_disp),
                                50, 50,                  // origin
                                g_winWidth, g_winHeight, // size
                                0, black,                // border
                                white );                 // backgd

    XMapWindow(g_disp, g_win);
    //eventMask = StructureNotifyMask | MapNotify | VisibilityChangeMask;
    eventMask = StructureNotifyMask | VisibilityChangeMask;
    XSelectInput(g_disp, g_win, eventMask);

    g_gc = XCreateGC(g_disp, g_win,
                     0,                       // mask of values
                     NULL );                  // array of values
#if 0

    do
    {
        dprint("about to call XNextEvent(...)\n");
        XNextEvent(g_disp, &evt);// calls XFlush
        dprint("returned from XNextEvent(...)\n");
    }

    //while(evt.type != MapNotify);
    while (evt.type != VisibilityNotify)
    {
        ;
    }

#endif

    // get access to the screen's color map
    colormap = DefaultColormap(g_disp, screenNumber);

    // alloc red color
    rc = XAllocNamedColor(g_disp, colormap, "red", &g_colors[0], &g_colors[0]);

    if (rc == 0)
    {
        printf("XAllocNamedColor - failed to allocated 'red' color.\n");
        exit(1);
    }

    rc = XAllocNamedColor(g_disp, colormap, "green", &g_colors[1], &g_colors[1]);

    if (rc == 0)
    {
        printf("XAllocNamedColor - failed to allocated 'green' color.\n");
        exit(1);
    }

    rc = XAllocNamedColor(g_disp, colormap, "blue", &g_colors[2], &g_colors[2]);

    if (rc == 0)
    {
        printf("XAllocNamedColor - failed to allocated 'blue' color.\n");
        exit(1);
    }

    rc = XAllocNamedColor(g_disp, colormap, "yellow", &g_colors[3], &g_colors[3]);

    if (rc == 0)
    {
        printf("XAllocNamedColor - failed to allocated 'yellow' color.\n");
        exit(1);
    }

    rc = XAllocNamedColor(g_disp, colormap, "orange", &g_colors[4], &g_colors[4]);

    if (rc == 0)
    {
        printf("XAllocNamedColor - failed to allocated 'orange' color.\n");
        exit(1);
    }

    if (zero_counters)
    {
        signal_tcp_proxy(proxy_app);
    }

    if (draw_lines)
    {
        start_timer(&tv);
        drawLines(iters);
        printf("drew %d lines in %d ms\n", iters, time_elapsed_ms(tv));
    }

    if (draw_rects)
    {
        start_timer(&tv);
        drawRectangles(iters);
        printf("drew %d rects in %d ms\n", iters, time_elapsed_ms(tv));
    }

    if (draw_stipples)
    {
        start_timer(&tv);
        // LK_TODO
    }

    if (draw_fonts)
    {
        start_timer(&tv);
        drawFont(iters, msg);
        printf("drew %d strings in %d ms\n", iters, time_elapsed_ms(tv));
    }

    if (draw_image)
    {
        start_timer(&tv);
        drawBMP(image_file, scroll_type);
        printf("drew BMP in %d ms\n", time_elapsed_ms(tv));
    }

    if (draw_offscreen)
    {
    }

    if (zero_counters)
    {
        signal_tcp_proxy(proxy_app);
    }

    eventMask = ButtonPressMask | ButtonReleaseMask | KeyPressMask;

    XSelectInput(g_disp, g_win, eventMask);

    do
    {
        XNextEvent(g_disp, &evt); // calls XFlush()

        if (evt.type == KeyPress)
        {
            if (draw_offscreen)
            {
                drawoffscreen();
            }
        }

    }
    while (evt.type != ButtonRelease);

    XDestroyWindow(g_disp, g_win);
    XCloseDisplay(g_disp);

    return 0;
}

int drawBMP(char *filename, int scroll_type)
{
    struct   pic_info pic_info;
    XImage   *image;
    Visual   *visual;
    Pixmap   pixmap;
    int      depth;
    int      i;
    int      j;

    if (parse_bmp(filename, &pic_info) < 0)
    {
        exit(-1);
    }

    XClearArea(g_disp, g_win, 0, 0, g_winWidth, g_winHeight, 0);

    depth = DefaultDepth(g_disp, DefaultScreen(g_disp));
    visual = DefaultVisual(g_disp, DefaultScreen(g_disp));

    // create empty pixmap
    pixmap = XCreatePixmap(g_disp, g_win, pic_info.width, pic_info.height, depth);

    // create an image from pixel data
    image = XCreateImage(g_disp, visual, depth, ZPixmap, 0, pic_info.pixel_data,
                         pic_info.width, pic_info.height, 32, 0);

    if (pic_info.height <= g_winHeight)
    {
        // image is too small to scroll
        XFlush(g_disp);
        XPutImage(g_disp, g_win, g_gc, image, 0, 0, 0, 0, pic_info.width, pic_info.height);
        XFlush(g_disp);
        return 0;
    }

    // copy image to pixelmap
    XPutImage(g_disp, pixmap, g_gc, image, 0, 0, 0, 0, pic_info.width, pic_info.height);

    if (scroll_type == SCROLL_JUMP)
    {

        if (pic_info.height <= g_winHeight)
        {
            // image too small - no scrolling required
            XFlush(g_disp);
            XCopyArea(g_disp,           // connection to X server
                      pixmap,           // source drawable
                      g_win,            // dest drawable
                      g_gc,             // graphics context
                      0, 0,             // source x,y
                      pic_info.width,   // width
                      pic_info.height,  // height
                      0, 0);            // dest x,y
            XFlush(g_disp);
            return 0;
        }

        j = pic_info.height / g_winHeight;

        if (pic_info.height % g_winHeight != 0)
        {
            // need to include the last part of the image
            j++;
        }

        XFlush(g_disp);

        for (i = 0; i < j; i++)
        {
            XCopyArea(g_disp,               // connection to X server
                      pixmap,               // source drawable
                      g_win,                // dest drawable
                      g_gc,                 // graphics context
                      0, i * g_winHeight,   // source x,y
                      pic_info.width,       // width
                      pic_info.height,      // height
                      0, 0);                // dest x,y
            XFlush(g_disp);
            sleep(3);
        }
    }

    /*
    ** smooth scroll the image
    */

    // number of lines to be scrolled
    j = pic_info.height - g_winHeight;

    if (scroll_type == SCROLL_SMOOTH1)
    {
        printf("running SCROLL_SMOOTH1\n");
        XFlush(g_disp);
        XPutImage(g_disp, g_win, g_gc, image, 0, 0, 0, 0, pic_info.width, pic_info.height);
        XFlush(g_disp);
        usleep(10000);

        for (i = 0; i < j; i++)
        {
            XCopyArea(g_disp, g_win, g_win, g_gc, 0, 1, g_winWidth, g_winHeight - 1, 0, 0);
            XPutImage(g_disp, g_win, g_gc, image, 0, g_winHeight + i, 0, g_winHeight - 1 , pic_info.width, 1);
            XFlush(g_disp);
            usleep(10000);
        }

        return 0;
    }

    if (scroll_type == SCROLL_SMOOTH2)
    {
        printf("running SCROLL_SMOOTH2\n");
        XFlush(g_disp);

        for (i = 0; i < j; i++)
        {
            XPutImage(g_disp, g_win, g_gc, image, 0, i, 0, 0, pic_info.width, pic_info.height - i);
            XFlush(g_disp);
            usleep(10000);
        }
    }

    if (scroll_type == SCROLL_SMOOTH3)
    {
        printf("running SCROLL_SMOOTH3\n");
        XFlush(g_disp);
        XCopyArea(g_disp, pixmap, g_win, g_gc, 0, 0, pic_info.width, pic_info.height, 0, 0);
        XFlush(g_disp);
        usleep(10000);

        for (i = 0; i < j; i++)
        {
            XCopyArea(g_disp, g_win, g_win, g_gc, 0, 1, g_winWidth, g_winHeight - 1, 0, 0);
            XCopyArea(g_disp, pixmap, g_win, g_gc, 0, g_winHeight + i, pic_info.width, 1, 0, g_winHeight - 1);
            XFlush(g_disp);
            usleep(10000);
        }

        return 0;
    }

    if (scroll_type == SCROLL_SMOOTH4)
    {
        printf("running SCROLL_SMOOTH4\n");
        XFlush(g_disp);

        for (i = 0; i < j; i++)
        {
            XCopyArea(g_disp, pixmap, g_win, g_gc, 0, i, pic_info.width, pic_info.height - i, 0, 0);
            XFlush(g_disp);
            usleep(10000);
        }
    }

    return 0;
}

int process_bmp_event(void)
{
    XEvent ev;
    long   event_mask;

    event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | StructureNotifyMask;
    XSelectInput(g_disp, g_win, event_mask);
    XNextEvent(g_disp, &ev);

    switch (ev.type)
    {
        case Expose:
            printf("got expose event\n");
            break;

        default:
            printf("did not get expose event\n");
            break;
    }

    return 0;
}

/**
 * send a SIGUSR1 to process tcp_proxy, causing it to clear counters
 *
 * @return 0 on success, -1 on failure
 */

int signal_tcp_proxy(char *proc_name)
{
    FILE *fp;
    char *cptr;
    char buf[2048];
    int  pids[10];
    int  status = 0;
    int  num_procs;
    int  i;

    sprintf(buf, "pidof %s", proc_name);

    if ((fp = popen(buf, "r")) == NULL )
    {
        printf("xdemo: popen() failed\n");
        return -1;
    }

    cptr = fgets(buf, 2047, fp);

    if (cptr == NULL)
    {
        pclose(fp);
        return -1;
    }

    num_procs = sscanf(buf, "%d %d %d %d %d %d %d %d %d %d",
                       &pids[0], &pids[1], &pids[2], &pids[3], &pids[4],
                       &pids[5], &pids[6], &pids[7], &pids[8], &pids[9]);

    if (num_procs > 0)
    {
        for (i = 0; i < num_procs; i++)
        {
            kill(pids[i], SIGUSR1);
            printf("sent SIGUSR1 to process %d\n", pids[i]);
        }
    }

    pclose(fp);
    return status;
}

