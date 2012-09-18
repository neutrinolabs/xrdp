/*
Copyright 2005-2012 Jay Sorg

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

*/

#include "rdp.h"
#include "xrdp_rail.h"

#define LOG_LEVEL 1
#define LLOG(_level, _args) \
  do { if (_level < LOG_LEVEL) { ErrorF _args ; } } while (0)
#define LLOGLN(_level, _args) \
  do { if (_level < LOG_LEVEL) { ErrorF _args ; ErrorF("\n"); } } while (0)

int g_con_number = 0; /* increments for each connection */

static int g_listen_sck = 0;
static int g_sck = 0;
static int g_sck_closed = 0;
static int g_connected = 0;
static int g_dis_listen_sck = 0;
//static int g_dis_sck = 0;
//static int g_dis_sck_closed = 0;
//static int g_dis_connected = 0;

static int g_begin = 0;
static struct stream* g_out_s = 0;
static struct stream* g_in_s = 0;
static int g_button_mask = 0;
static int g_cursor_x = 0;
static int g_cursor_y = 0;
static OsTimerPtr g_timer = 0;
static int g_scheduled = 0;
static int g_count = 0;
static int g_rdpindex = -1;

extern DevPrivateKeyRec g_rdpWindowIndex; /* from rdpmain.c */
extern ScreenPtr g_pScreen; /* from rdpmain.c */
extern int g_Bpp; /* from rdpmain.c */
extern int g_Bpp_mask; /* from rdpmain.c */
extern rdpScreenInfoRec g_rdpScreen; /* from rdpmain.c */
extern int g_use_rail; /* from rdpmain.c */

/* true is to use unix domain socket */
extern int g_use_uds; /* in rdpmain.c */
extern char g_uds_data[]; /* in rdpmain.c */
extern int g_do_dirty_ons; /* in rdpmain.c */
extern rdpPixmapRec g_screenPriv; /* in rdpmain.c */

struct rdpup_os_bitmap
{
  int used;
  PixmapPtr pixmap;
  rdpPixmapPtr priv;
  int stamp;
};

static struct rdpup_os_bitmap* g_os_bitmaps = 0;
static int g_max_os_bitmaps = 0;
static int g_os_bitmap_stamp = 0;

static int g_pixmap_byte_total = 0;
static int g_pixmap_num_used = 0;

struct rdpup_top_window
{
  WindowPtr wnd;
  struct rdpup_top_window* next;
};

/*
0 GXclear,        0
1 GXnor,          DPon
2 GXandInverted,  DPna
3 GXcopyInverted, Pn
4 GXandReverse,   PDna
5 GXinvert,       Dn
6 GXxor,          DPx
7 GXnand,         DPan
8 GXand,          DPa
9 GXequiv,        DPxn
a GXnoop,         D
b GXorInverted,   DPno
c GXcopy,         P
d GXorReverse,   PDno
e GXor,          DPo
f GXset          1
*/

static int g_rdp_opcodes[16] =
{
  0x00, /* GXclear        0x0 0 */
  0x88, /* GXand          0x1 src AND dst */
  0x44, /* GXandReverse   0x2 src AND NOT dst */
  0xcc, /* GXcopy         0x3 src */
  0x22, /* GXandInverted  0x4 NOT src AND dst */
  0xaa, /* GXnoop         0x5 dst */
  0x66, /* GXxor          0x6 src XOR dst */
  0xee, /* GXor           0x7 src OR dst */
  0x11, /* GXnor          0x8 NOT src AND NOT dst */
  0x99, /* GXequiv        0x9 NOT src XOR dst */
  0x55, /* GXinvert       0xa NOT dst */
  0xdd, /* GXorReverse    0xb src OR NOT dst */
  0x33, /* GXcopyInverted 0xc NOT src */
  0xbb, /* GXorInverted   0xd NOT src OR dst */
  0x77, /* GXnand         0xe NOT src OR NOT dst */
  0xff  /* GXset          0xf 1 */
};

/*****************************************************************************/
static int
rdpup_disconnect(void)
{
  int index;

  RemoveEnabledDevice(g_sck);
  g_connected = 0;
  g_tcp_close(g_sck);
  g_sck = 0;
  g_sck_closed = 1;
  g_pixmap_byte_total = 0;
  g_pixmap_num_used = 0;
  if (g_max_os_bitmaps > 0)
  {
    for (index = 0; index < g_max_os_bitmaps; index++)
    {
      if (g_os_bitmaps[index].used)
      {
        if (g_os_bitmaps[index].priv != 0)
        {
          g_os_bitmaps[index].priv->status = 0;
        }
      }
    }
  }
  g_max_os_bitmaps = 0;
  g_free(g_os_bitmaps);
  g_os_bitmaps = 0;
  g_use_rail = 0;
  return 0;
}

/*****************************************************************************/
int
rdpup_add_os_bitmap(PixmapPtr pixmap, rdpPixmapPtr priv)
{
  int index;
  int rv;
  int oldest;
  int oldest_index;

  if (!g_connected)
  {
    return -1;
  }
  if (g_os_bitmaps == 0)
  {
    return -1;
  }
  rv = -1;
  index = 0;
  while (index < g_max_os_bitmaps)
  {
    if (g_os_bitmaps[index].used == 0)
    {
      g_os_bitmaps[index].used = 1;
      g_os_bitmaps[index].pixmap = pixmap;
      g_os_bitmaps[index].priv = priv;
      g_os_bitmaps[index].stamp = g_os_bitmap_stamp;
      g_os_bitmap_stamp++;
      g_pixmap_num_used++;
      rv = index;
      break;
    }
    index++;
  }
  if (rv == -1)
  {
    /* find oldest */
    oldest = 0x7fffffff;
    oldest_index = 0;
    index = 0;
    while (index < g_max_os_bitmaps)
    {
      if (g_os_bitmaps[index].stamp < oldest)
      {
        oldest = g_os_bitmaps[index].stamp;
        oldest_index = index;
      }
      index++;
    }
    LLOGLN(10, ("rdpup_add_os_bitmap: evicting old, oldest_index %d", oldest_index));
    /* evict old */
    g_os_bitmaps[oldest_index].priv->status = 0;
    /* set new */
    g_os_bitmaps[oldest_index].pixmap = pixmap;
    g_os_bitmaps[oldest_index].priv = priv;
    g_os_bitmaps[oldest_index].stamp = g_os_bitmap_stamp;
    g_os_bitmap_stamp++;
    rv = oldest_index;
  }
  LLOGLN(10, ("rdpup_add_os_bitmap: new bitmap index %d", rv));
  LLOGLN(10, ("  g_pixmap_num_used %d", g_pixmap_num_used));
  return rv;
}

/*****************************************************************************/
int
rdpup_remove_os_bitmap(int rdpindex)
{
  LLOGLN(10, ("rdpup_remove_os_bitmap: index %d stamp %d",
         rdpindex, g_os_bitmaps[rdpindex].stamp));
  if (g_os_bitmaps == 0)
  {
    return 1;
  }
  if ((rdpindex < 0) && (rdpindex >= g_max_os_bitmaps))
  {
    return 1;
  }
  if (g_os_bitmaps[rdpindex].used)
  {
    g_os_bitmaps[rdpindex].used = 0;
    g_os_bitmaps[rdpindex].pixmap = 0;
    g_os_bitmaps[rdpindex].priv = 0;
    g_pixmap_num_used--;
  }
  LLOGLN(10, ("  g_pixmap_num_used %d", g_pixmap_num_used));
  return 0;
}

/*****************************************************************************/
/* returns error */
static int
rdpup_send(char* data, int len)
{
  int sent;

  LLOGLN(10, ("rdpup_send - sending %d bytes", len));
  if (g_sck_closed)
  {
    return 1;
  }
  while (len > 0)
  {
    sent = g_tcp_send(g_sck, data, len, 0);
    if (sent == -1)
    {
      if (g_tcp_last_error_would_block(g_sck))
      {
        g_sleep(1);
      }
      else
      {
        rdpup_disconnect();
        return 1;
      }
    }
    else if (sent == 0)
    {
      rdpup_disconnect();
      return 1;
    }
    else
    {
      data += sent;
      len -= sent;
    }
  }
  return 0;
}

/******************************************************************************/
static int
rdpup_send_msg(struct stream* s)
{
  int len;
  int rv;

  rv = 1;
  if (s != 0)
  {
    len = (int)(s->end - s->data);
    if (len > s->size)
    {
      rdpLog("overrun error len %d count %d\n", len, g_count);
    }
    s_pop_layer(s, iso_hdr);
    out_uint16_le(s, 3);
    out_uint16_le(s, g_count);
    out_uint32_le(s, len - 8);
    rv = rdpup_send(s->data, len);
  }
  if (rv != 0)
  {
    rdpLog("error in rdpup_send_msg\n");
  }
  return rv;
}

/******************************************************************************/
static int
rdpup_send_pending(void)
{
  if (g_connected && g_begin)
  {
    LLOGLN(10, ("end %d", g_count));
    out_uint16_le(g_out_s, 2);
    out_uint16_le(g_out_s, 4);
    g_count++;
    s_mark_end(g_out_s);
    rdpup_send_msg(g_out_s);
  }
  g_count = 0;
  g_begin = 0;
  return 0;
}

/******************************************************************************/
static CARD32
rdpDeferredUpdateCallback(OsTimerPtr timer, CARD32 now, pointer arg)
{
  LLOGLN(10, ("rdpDeferredUpdateCallback"));
  if (g_do_dirty_ons)
  {
    rdpup_check_dirty_screen(&g_screenPriv);
  }
  else
  {
    rdpup_send_pending();
  }
  g_scheduled = 0;
  return 0;
}

/******************************************************************************/
void
rdpScheduleDeferredUpdate(void)
{
  if (!g_scheduled)
  {
    g_scheduled = 1;
    g_timer = TimerSet(g_timer, 0, 40, rdpDeferredUpdateCallback, 0);
  }
}

/******************************************************************************/
/* returns error */
static int
rdpup_recv(char* data, int len)
{
  int rcvd;

  if (g_sck_closed)
  {
    return 1;
  }
  while (len > 0)
  {
    rcvd = g_tcp_recv(g_sck, data, len, 0);
    if (rcvd == -1)
    {
      if (g_tcp_last_error_would_block(g_sck))
      {
        g_sleep(1);
      }
      else
      {
        rdpup_disconnect();
        return 1;
      }
    }
    else if (rcvd == 0)
    {
      rdpup_disconnect();
      return 1;
    }
    else
    {
      data += rcvd;
      len -= rcvd;
    }
  }
  return 0;
}

/******************************************************************************/
static int
rdpup_recv_msg(struct stream* s)
{
  int len;
  int rv;

  rv = 1;
  if (s != 0)
  {
    init_stream(s, 4);
    rv = rdpup_recv(s->data, 4);
    if (rv == 0)
    {
      in_uint32_le(s, len);
      if (len > 3)
      {
        init_stream(s, len);
        rv = rdpup_recv(s->data, len - 4);
      }
    }
  }
  if (rv != 0)
  {
    rdpLog("error in rdpup_recv_msg\n");
  }
  return rv;
}

/******************************************************************************/
/*
    this from miScreenInit
    pScreen->mmWidth = (xsize * 254 + dpix * 5) / (dpix * 10);
    pScreen->mmHeight = (ysize * 254 + dpiy * 5) / (dpiy * 10);
*/
static int
process_screen_size_msg(int width, int height, int bpp)
{
  RRScreenSizePtr pSize;
  int mmwidth;
  int mmheight;
  Bool ok;

  LLOGLN(0, ("process_screen_size_msg: set width %d height %d bpp %d",
         width, height, bpp));
  g_rdpScreen.rdp_width = width;
  g_rdpScreen.rdp_height = height;
  g_rdpScreen.rdp_bpp = bpp;
  if (bpp < 15)
  {
    g_rdpScreen.rdp_Bpp = 1;
    g_rdpScreen.rdp_Bpp_mask = 0xff;
  }
  else if (bpp == 15)
  {
    g_rdpScreen.rdp_Bpp = 2;
    g_rdpScreen.rdp_Bpp_mask = 0x7fff;
  }
  else if (bpp == 16)
  {
    g_rdpScreen.rdp_Bpp = 2;
    g_rdpScreen.rdp_Bpp_mask = 0xffff;
  }
  else if (bpp > 16)
  {
    g_rdpScreen.rdp_Bpp = 4;
    g_rdpScreen.rdp_Bpp_mask = 0xffffff;
  }
  mmwidth = PixelToMM(width);
  mmheight = PixelToMM(height);

  pSize = RRRegisterSize(g_pScreen, width, height, mmwidth, mmheight);
  RRSetCurrentConfig(g_pScreen, RR_Rotate_0, 0, pSize);
  if ((g_rdpScreen.width != width) || (g_rdpScreen.height != height))
  {
    LLOGLN(0, ("  calling RRScreenSizeSet"));
    ok = RRScreenSizeSet(g_pScreen, width, height, mmwidth, mmheight);
    LLOGLN(0, ("  RRScreenSizeSet ok=[%d]", ok));
  }
  return 0;
}

/******************************************************************************/
static int
l_bound_by(int val, int low, int high)
{
  if (val > high)
  {
    val = high;
  }
  if (val < low)
  {
    val = low;
  }
  return val;
}

/******************************************************************************/
static int
rdpup_send_caps(void)
{
  struct stream* ls;
  int len;
  int rv;
  int cap_count;
  int cap_bytes;

  make_stream(ls);
  init_stream(ls, 8192);
  s_push_layer(ls, iso_hdr, 8);

  cap_count = 0;
  cap_bytes = 0;

#if 0
  out_uint16_le(ls, 0);
  out_uint16_le(ls, 4);
  cap_count++;
  cap_bytes += 4;

  out_uint16_le(ls, 1);
  out_uint16_le(ls, 4);
  cap_count++;
  cap_bytes += 4;
#endif

  s_mark_end(ls);
  len = (int)(ls->end - ls->data);
  s_pop_layer(ls, iso_hdr);
  out_uint16_le(ls, 2); /* caps */
  out_uint16_le(ls, cap_count); /* num caps */
  out_uint32_le(ls, cap_bytes); /* caps len after header */

  rv = rdpup_send(ls->data, len);
  if (rv != 0)
  {
    LLOGLN(0, ("rdpup_send_caps: rdpup_send failed"));
  }
  free_stream(ls);
  return rv;
}

/******************************************************************************/
static int
process_version_msg(int param1, int param2, int param3, int param4)
{
  LLOGLN(0, ("process_version_msg: version %d %d %d %d", param1, param2,
         param3, param4));
  if ((param1 > 0) || (param2 > 0) || (param3 > 0) || (param4 > 0))
  {
    rdpup_send_caps();
  }
  return 0;
}

/******************************************************************************/
static int
rdpup_send_rail(void)
{
  WindowPtr wnd;
  rdpWindowRec* priv;

  wnd = g_pScreen->root;
  if (wnd != 0)
  {
    wnd = wnd->lastChild;
    while (wnd != 0)
    {
      if (wnd->realized)
      {
        priv = GETWINPRIV(wnd);
        priv->status = 1;
        rdpup_create_window(wnd, priv);
      }
      wnd = wnd->prevSib;
    }
  }
  return 0;
}

/******************************************************************************/
static int
rdpup_process_msg(struct stream* s)
{
  int msg_type;
  int msg;
  int param1;
  int param2;
  int param3;
  int param4;
  int bytes;
  int i1;

  in_uint16_le(s, msg_type);
  if (msg_type == 103)
  {
    in_uint32_le(s, msg);
    in_uint32_le(s, param1);
    in_uint32_le(s, param2);
    in_uint32_le(s, param3);
    in_uint32_le(s, param4);
    LLOGLN(10, ("rdpup_process_msg - msg %d param1 %d param2 %d param3 %d "
           "param4 %d", msg, param1, param2, param3, param4));
    switch (msg)
    {
      case 15: /* key down */
      case 16: /* key up */
        KbdAddEvent(msg == 15, param1, param2, param3, param4);
        break;
      case 17: /* from RDP_INPUT_SYNCHRONIZE */
        KbdSync(param1);
        break;
      case 100:
        /* without the minus 2, strange things happen when dragging
           past the width or height */
        g_cursor_x = l_bound_by(param1, 0, g_rdpScreen.width - 2);
        g_cursor_y = l_bound_by(param2, 0, g_rdpScreen.height - 2);
        PtrAddEvent(g_button_mask, g_cursor_x, g_cursor_y);
        break;
      case 101:
        g_button_mask = g_button_mask & (~1);
        PtrAddEvent(g_button_mask, g_cursor_x, g_cursor_y);
        break;
      case 102:
        g_button_mask = g_button_mask | 1;
        PtrAddEvent(g_button_mask, g_cursor_x, g_cursor_y);
        break;
      case 103:
        g_button_mask = g_button_mask & (~4);
        PtrAddEvent(g_button_mask, g_cursor_x, g_cursor_y);
        break;
      case 104:
        g_button_mask = g_button_mask | 4;
        PtrAddEvent(g_button_mask, g_cursor_x, g_cursor_y);
        break;
      case 105:
        g_button_mask = g_button_mask & (~2);
        PtrAddEvent(g_button_mask, g_cursor_x, g_cursor_y);
        break;
      case 106:
        g_button_mask = g_button_mask | 2;
        PtrAddEvent(g_button_mask, g_cursor_x, g_cursor_y);
        break;
      case 107:
        g_button_mask = g_button_mask & (~8);
        PtrAddEvent(g_button_mask, g_cursor_x, g_cursor_y);
        break;
      case 108:
        g_button_mask = g_button_mask | 8;
        PtrAddEvent(g_button_mask, g_cursor_x, g_cursor_y);
        break;
      case 109:
        g_button_mask = g_button_mask & (~16);
        PtrAddEvent(g_button_mask, g_cursor_x, g_cursor_y);
        break;
      case 110:
        g_button_mask = g_button_mask | 16;
        PtrAddEvent(g_button_mask, g_cursor_x, g_cursor_y);
        break;
      case 200:
        rdpup_begin_update();
        rdpup_send_area(0, (param1 >> 16) & 0xffff, param1 & 0xffff,
                        (param2 >> 16) & 0xffff, param2 & 0xffff);
        rdpup_end_update();
        break;
      case 300:
        process_screen_size_msg(param1, param2, param3);
        break;
      case 301:
        process_version_msg(param1, param2, param3, param4);
        break;
    }
  }
  else if (msg_type == 104)
  {
    in_uint32_le(s, bytes);
    if (bytes > sizeof(g_rdpScreen.client_info))
    {
      bytes = sizeof(g_rdpScreen.client_info);
    }
    memcpy(&(g_rdpScreen.client_info), s->p - 4, bytes);
    g_rdpScreen.client_info.size = bytes;
    LLOGLN(0, ("rdpup_process_msg: got client info bytes %d", bytes));
    LLOGLN(0, ("  jpeg support %d", g_rdpScreen.client_info.jpeg));
    i1 = g_rdpScreen.client_info.offscreen_support_level;
    LLOGLN(0, ("  offscreen support %d", i1));
    i1 = g_rdpScreen.client_info.offscreen_cache_size;
    LLOGLN(0, ("  offscreen size %d", i1));
    i1 = g_rdpScreen.client_info.offscreen_cache_entries;
    LLOGLN(0, ("  offscreen entries %d", i1));
    if (g_rdpScreen.client_info.offscreen_support_level > 0)
    {
      if (g_rdpScreen.client_info.offscreen_cache_entries > 0)
      {
        g_max_os_bitmaps = g_rdpScreen.client_info.offscreen_cache_entries;
        g_free(g_os_bitmaps);
        g_os_bitmaps = (struct rdpup_os_bitmap*)
               g_malloc(sizeof(struct rdpup_os_bitmap) * g_max_os_bitmaps, 1);
      }
    }
    if (g_rdpScreen.client_info.rail_support_level > 0)
    {
      g_use_rail = 1;
      rdpup_send_rail();
    }
  }
  else
  {
    rdpLog("unknown message type in rdpup_process_msg %d\n", msg_type);
  }
  return 0;
}

/******************************************************************************/
void
rdpup_get_screen_image_rect(struct image_data* id)
{
  id->width = g_rdpScreen.width;
  id->height = g_rdpScreen.height;
  id->bpp = g_rdpScreen.rdp_bpp;
  id->Bpp = g_rdpScreen.rdp_Bpp;
  id->lineBytes = g_rdpScreen.paddedWidthInBytes;
  id->pixels = g_rdpScreen.pfbMemory;
}

/******************************************************************************/
void
rdpup_get_pixmap_image_rect(PixmapPtr pPixmap, struct image_data* id)
{
  id->width = pPixmap->drawable.width;
  id->height = pPixmap->drawable.height;
  id->bpp = g_rdpScreen.rdp_bpp;
  id->Bpp = g_rdpScreen.rdp_Bpp;
  id->lineBytes = pPixmap->devKind;
  id->pixels = (char*)(pPixmap->devPrivate.ptr);
}

/******************************************************************************/
int
rdpup_init(void)
{
  char text[256];
  int i;

  if (!g_directory_exist("/tmp/.xrdp"))
  {
    if (!g_create_dir("/tmp/.xrdp"))
    {
      LLOGLN(0, ("rdpup_init: g_create_dir failed"));
      return 0;
    }
    g_chmod_hex("/tmp/.xrdp", 0x1777);
  }
  i = atoi(display);
  if (i < 1)
  {
    return 0;
  }
  if (g_in_s == 0)
  {
    make_stream(g_in_s);
    init_stream(g_in_s, 8192);
  }
  if (g_out_s == 0)
  {
    make_stream(g_out_s);
    init_stream(g_out_s, 8192 * g_Bpp + 100);
  }
  if (g_use_uds)
  {
    g_sprintf(g_uds_data, "/tmp/.xrdp/xrdp_display_%s", display);
    if (g_listen_sck == 0)
    {
      g_listen_sck = g_tcp_local_socket_stream();
      if (g_tcp_local_bind(g_listen_sck, g_uds_data) != 0)
      {
        LLOGLN(0, ("rdpup_init: g_tcp_local_bind failed"));
        return 0;
      }
      g_tcp_listen(g_listen_sck);
      AddEnabledDevice(g_listen_sck);
    }
  }
  else
  {
    g_sprintf(text, "62%2.2d", i);
    if (g_listen_sck == 0)
    {
      g_listen_sck = g_tcp_socket();
      if (g_tcp_bind(g_listen_sck, text) != 0)
      {
        return 0;
      }
      g_tcp_listen(g_listen_sck);
      AddEnabledDevice(g_listen_sck);
    }
  }
  g_dis_listen_sck = g_tcp_local_socket_dgram();
  if (g_dis_listen_sck != 0)
  {
    g_sprintf(text, "/tmp/.xrdp/xrdp_disconnect_display_%s", display);
    if (g_tcp_local_bind(g_dis_listen_sck, text) == 0)
    {
      AddEnabledDevice(g_dis_listen_sck);
    }
    else
    {
      rdpLog("g_tcp_local_bind failed [%s]\n", text);
    }
  }
  return 1;
}

/******************************************************************************/
int
rdpup_check(void)
{
  int sel;
  int new_sck;
  char buf[8];

  sel = g_tcp_select(g_listen_sck, g_sck, g_dis_listen_sck);
  if (sel & 1)
  {
    new_sck = g_tcp_accept(g_listen_sck);
    if (new_sck == -1)
    {
    }
    else
    {
      if (g_sck != 0)
      {
        /* should maybe ask is user wants to allow here with timeout */
        rdpLog("replacing connection, already got a connection\n");
        rdpup_disconnect();
      }
      rdpLog("got a connection\n");
      g_sck = new_sck;
      g_tcp_set_non_blocking(g_sck);
      g_tcp_set_no_delay(g_sck);
      g_connected = 1;
      g_sck_closed = 0;
      g_begin = 0;
      g_con_number++;
      AddEnabledDevice(g_sck);
    }
  }
  if (sel & 2)
  {
    if (rdpup_recv_msg(g_in_s) == 0)
    {
      rdpup_process_msg(g_in_s);
    }
  }
  if (sel & 4)
  {
    if (g_tcp_recv(g_dis_listen_sck, buf, 4, 0) > 0)
    {
      if (g_sck != 0)
      {
        rdpLog("disconnecting session via user request\n");
        rdpup_disconnect();
      }
    }
  }
  return 0;
}

/******************************************************************************/
int
rdpup_begin_update(void)
{
  LLOGLN(10, ("rdpup_begin_update"));
  if (g_connected)
  {
    if (g_begin)
    {
      return 0;
    }
    init_stream(g_out_s, 0);
    s_push_layer(g_out_s, iso_hdr, 8);
    out_uint16_le(g_out_s, 1); /* begin update */
    out_uint16_le(g_out_s, 4); /* size */
    LLOGLN(10, ("begin %d", g_count));
    g_begin = 1;
    g_count = 1;
  }
  return 0;
}

/******************************************************************************/
int
rdpup_end_update(void)
{
  LLOGLN(10, ("rdpup_end_update"));
  if (g_connected && g_begin)
  {
    if (g_do_dirty_ons)
    {
      rdpup_send_pending();
    }
    else
    {
      rdpScheduleDeferredUpdate();
    }
  }
  return 0;
}

/******************************************************************************/
int
rdpup_pre_check(int in_size)
{
  if (!g_begin)
  {
    rdpup_begin_update();
  }
  if ((g_out_s->p - g_out_s->data) > (g_out_s->size - (in_size + 20)))
  {
    s_mark_end(g_out_s);
    rdpup_send_msg(g_out_s);
    g_count = 0;
    init_stream(g_out_s, 0);
    s_push_layer(g_out_s, iso_hdr, 8);
  }
  return 0;
}

/******************************************************************************/
int
rdpup_fill_rect(short x, short y, int cx, int cy)
{
  if (g_connected)
  {
    LLOGLN(10, ("  rdpup_fill_rect"));
    rdpup_pre_check(12);
    out_uint16_le(g_out_s, 3); /* fill rect */
    out_uint16_le(g_out_s, 12); /* size */
    g_count++;
    out_uint16_le(g_out_s, x);
    out_uint16_le(g_out_s, y);
    out_uint16_le(g_out_s, cx);
    out_uint16_le(g_out_s, cy);
  }
  return 0;
}

/******************************************************************************/
int
rdpup_screen_blt(short x, short y, int cx, int cy, short srcx, short srcy)
{
  if (g_connected)
  {
    LLOGLN(10, ("  rdpup_screen_blt"));
    rdpup_pre_check(16);
    out_uint16_le(g_out_s, 4); /* screen blt */
    out_uint16_le(g_out_s, 16); /* size */
    g_count++;
    out_uint16_le(g_out_s, x);
    out_uint16_le(g_out_s, y);
    out_uint16_le(g_out_s, cx);
    out_uint16_le(g_out_s, cy);
    out_uint16_le(g_out_s, srcx);
    out_uint16_le(g_out_s, srcy);
  }
  return 0;
}

/******************************************************************************/
int
rdpup_set_clip(short x, short y, int cx, int cy)
{
  if (g_connected)
  {
    LLOGLN(10, ("  rdpup_set_clip"));
    rdpup_pre_check(12);
    out_uint16_le(g_out_s, 10); /* set clip */
    out_uint16_le(g_out_s, 12); /* size */
    g_count++;
    out_uint16_le(g_out_s, x);
    out_uint16_le(g_out_s, y);
    out_uint16_le(g_out_s, cx);
    out_uint16_le(g_out_s, cy);
  }
  return 0;
}

/******************************************************************************/
int
rdpup_reset_clip(void)
{
  if (g_connected)
  {
    LLOGLN(10, ("  rdpup_reset_clip"));
    rdpup_pre_check(4);
    out_uint16_le(g_out_s, 11); /* reset clip */
    out_uint16_le(g_out_s, 4); /* size */
    g_count++;
  }
  return 0;
}

#define COLOR8(r, g, b) \
  ((((r) >> 5) << 0)  | (((g) >> 5) << 3) | (((b) >> 6) << 6))
#define COLOR15(r, g, b) \
  ((((r) >> 3) << 10) | (((g) >> 3) << 5) | (((b) >> 3) << 0))
#define COLOR16(r, g, b) \
  ((((r) >> 3) << 11) | (((g) >> 2) << 5) | (((b) >> 3) << 0))
#define COLOR24(r, g, b) \
  ((((r) >> 0) << 0)  | (((g) >> 0) << 8) | (((b) >> 0) << 16))
#define SPLITCOLOR32(r, g, b, c) \
{ \
  r = ((c) >> 16) & 0xff; \
  g = ((c) >> 8) & 0xff; \
  b = (c) & 0xff; \
}

int
convert_pixel(int in_pixel)
{
  int red;
  int green;
  int blue;
  int rv;

  rv = 0;
  if (g_rdpScreen.depth == 24)
  {
    if (g_rdpScreen.rdp_bpp == 24)
    {
      rv = in_pixel;
      SPLITCOLOR32(red, green, blue, rv);
      rv = COLOR24(red, green, blue);
    }
    else if (g_rdpScreen.rdp_bpp == 16)
    {
      rv = in_pixel;
      SPLITCOLOR32(red, green, blue, rv);
      rv = COLOR16(red, green, blue);
    }
    else if (g_rdpScreen.rdp_bpp == 15)
    {
      rv = in_pixel;
      SPLITCOLOR32(red, green, blue, rv);
      rv = COLOR15(red, green, blue);
    }
    else if (g_rdpScreen.rdp_bpp == 8)
    {
      rv = in_pixel;
      SPLITCOLOR32(red, green, blue, rv);
      rv = COLOR8(red, green, blue);
    }
  }
  else if (g_rdpScreen.depth == g_rdpScreen.rdp_bpp)
  {
    return in_pixel;
  }
  return rv;
}

int
convert_pixels(void* src, void* dst, int num_pixels)
{
  unsigned int pixel;
  unsigned int red;
  unsigned int green;
  unsigned int blue;
  unsigned int* src32;
  unsigned int* dst32;
  unsigned short* dst16;
  unsigned char* dst8;
  int index;

  if (g_rdpScreen.depth == g_rdpScreen.rdp_bpp)
  {
    memcpy(dst, src, num_pixels * g_Bpp);
    return 0;
  }
  if (g_rdpScreen.depth == 24)
  {
    src32 = (unsigned int*)src;
    if (g_rdpScreen.rdp_bpp == 24)
    {
      dst32 = (unsigned int*)dst;
      for (index = 0; index < num_pixels; index++)
      {
        pixel = *src32;
        *dst32 = pixel;
        dst32++;
        src32++;
      }
    }
    else if (g_rdpScreen.rdp_bpp == 16)
    {
      dst16 = (unsigned short*)dst;
      for (index = 0; index < num_pixels; index++)
      {
        pixel = *src32;
        SPLITCOLOR32(red, green, blue, pixel);
        pixel = COLOR16(red, green, blue);
        *dst16 = pixel;
        dst16++;
        src32++;
      }
    }
    else if (g_rdpScreen.rdp_bpp == 15)
    {
      dst16 = (unsigned short*)dst;
      for (index = 0; index < num_pixels; index++)
      {
        pixel = *src32;
        SPLITCOLOR32(red, green, blue, pixel);
        pixel = COLOR15(red, green, blue);
        *dst16 = pixel;
        dst16++;
        src32++;
      }
    }
    else if (g_rdpScreen.rdp_bpp == 8)
    {
      dst8 = (unsigned char*)dst;
      for (index = 0; index < num_pixels; index++)
      {
        pixel = *src32;
        SPLITCOLOR32(red, green, blue, pixel);
        pixel = COLOR8(red, green, blue);
        *dst8 = pixel;
        dst8++;
        src32++;
      }
    }
  }
  return 0;
}

/******************************************************************************/
int
rdpup_set_fgcolor(int fgcolor)
{
  if (g_connected)
  {
    LLOGLN(10, ("  rdpup_set_fgcolor"));
    rdpup_pre_check(8);
    out_uint16_le(g_out_s, 12); /* set fgcolor */
    out_uint16_le(g_out_s, 8); /* size */
    g_count++;
    fgcolor = fgcolor & g_Bpp_mask;
    fgcolor = convert_pixel(fgcolor) & g_rdpScreen.rdp_Bpp_mask;
    out_uint32_le(g_out_s, fgcolor);
  }
  return 0;
}

/******************************************************************************/
int
rdpup_set_bgcolor(int bgcolor)
{
  if (g_connected)
  {
    LLOGLN(10, ("  rdpup_set_bgcolor"));
    rdpup_pre_check(8);
    out_uint16_le(g_out_s, 13); /* set bg color */
    out_uint16_le(g_out_s, 8); /* size */
    g_count++;
    bgcolor = bgcolor & g_Bpp_mask;
    bgcolor = convert_pixel(bgcolor) & g_rdpScreen.rdp_Bpp_mask;
    out_uint32_le(g_out_s, bgcolor);
  }
  return 0;
}

/******************************************************************************/
int
rdpup_set_opcode(int opcode)
{
  if (g_connected)
  {
    LLOGLN(10, ("  rdpup_set_opcode"));
    rdpup_pre_check(6);
    out_uint16_le(g_out_s, 14); /* set opcode */
    out_uint16_le(g_out_s, 6); /* size */
    g_count++;
    out_uint16_le(g_out_s, g_rdp_opcodes[opcode & 0xf]);
  }
  return 0;
}

/******************************************************************************/
int
rdpup_set_pen(int style, int width)
{
  if (g_connected)
  {
    LLOGLN(10, ("  rdpup_set_pen"));
    rdpup_pre_check(8);
    out_uint16_le(g_out_s, 17); /* set pen */
    out_uint16_le(g_out_s, 8); /* size */
    g_count++;
    out_uint16_le(g_out_s, style);
    out_uint16_le(g_out_s, width);
  }
  return 0;
}

/******************************************************************************/
int
rdpup_draw_line(short x1, short y1, short x2, short y2)
{
  if (g_connected)
  {
    LLOGLN(10, ("  rdpup_draw_line"));
    rdpup_pre_check(12);
    out_uint16_le(g_out_s, 18); /* draw line */
    out_uint16_le(g_out_s, 12); /* size */
    g_count++;
    out_uint16_le(g_out_s, x1);
    out_uint16_le(g_out_s, y1);
    out_uint16_le(g_out_s, x2);
    out_uint16_le(g_out_s, y2);
  }
  return 0;
}

/******************************************************************************/
int
rdpup_set_cursor(short x, short y, char* cur_data, char* cur_mask)
{
  int size;

  if (g_connected)
  {
    LLOGLN(10, ("  rdpup_set_cursor"));
    size = 8 + 32 * (32 * 3) + 32 * (32 / 8);
    rdpup_pre_check(size);
    out_uint16_le(g_out_s, 19); /* set cursor */
    out_uint16_le(g_out_s, size); /* size */
    g_count++;
    x = MAX(0, x);
    x = MIN(31, x);
    y = MAX(0, y);
    y = MIN(31, y);
    out_uint16_le(g_out_s, x);
    out_uint16_le(g_out_s, y);
    out_uint8a(g_out_s, cur_data, 32 * (32 * 3));
    out_uint8a(g_out_s, cur_mask, 32 * (32 / 8));
  }
  return 0;
}

/******************************************************************************/
int
rdpup_create_os_surface(int rdpindex, int width, int height)
{
  LLOGLN(10, ("rdpup_create_os_surface:"));
  if (g_connected)
  {
    LLOGLN(10, ("  rdpup_create_os_surface width %d height %d", width, height));
    rdpup_pre_check(12);
    out_uint16_le(g_out_s, 20);
    out_uint16_le(g_out_s, 12);
    g_count++;
    out_uint32_le(g_out_s, rdpindex);
    out_uint16_le(g_out_s, width);
    out_uint16_le(g_out_s, height);
  }
  return 0;
}

/******************************************************************************/
int
rdpup_switch_os_surface(int rdpindex)
{
  LLOGLN(10, ("rdpup_switch_os_surface:"));
  if (g_connected)
  {
    if (g_rdpindex == rdpindex)
    {
      return 0;
    }
    g_rdpindex = rdpindex;
    LLOGLN(10, ("rdpup_switch_os_surface: rdpindex %d", rdpindex));
    /* switch surface */
    rdpup_pre_check(8);
    out_uint16_le(g_out_s, 21);
    out_uint16_le(g_out_s, 8);
    out_uint32_le(g_out_s, rdpindex);
    g_count++;
  }
  return 0;
}

/******************************************************************************/
int
rdpup_delete_os_surface(int rdpindex)
{
  LLOGLN(10, ("rdpup_delete_os_surface: rdpindex %d", rdpindex));
  if (g_connected)
  {
    LLOGLN(10, ("rdpup_delete_os_surface: rdpindex %d", rdpindex));
    rdpup_pre_check(8);
    out_uint16_le(g_out_s, 22);
    out_uint16_le(g_out_s, 8);
    g_count++;
    out_uint32_le(g_out_s, rdpindex);
  }
  return 0;
}

/******************************************************************************/
static int
get_single_color(struct image_data* id, int x, int y, int w, int h)
{
  int rv;
  int i;
  int j;
  int p;
  unsigned char* i8;
  unsigned short* i16;
  unsigned int* i32;

  p = 0;
  rv = -1;
  if (g_Bpp == 1)
  {
    for (i = 0; i < h; i++)
    {
      i8 = (unsigned char*)(id->pixels +
               ((y + i) * id->lineBytes) + (x * g_Bpp));
      if (i == 0)
      {
        p = *i8;
      }
      for (j = 0; j < w; j++)
      {
        if (i8[j] != p)
        {
          return -1;
        }
      }
    }
    rv = p;
  }
  else if (g_Bpp == 2)
  {
    for (i = 0; i < h; i++)
    {
      i16 = (unsigned short*)(id->pixels +
               ((y + i) * id->lineBytes) + (x * g_Bpp));
      if (i == 0)
      {
        p = *i16;
      }
      for (j = 0; j < w; j++)
      {
        if (i16[j] != p)
        {
          return -1;
        }
      }
    }
    rv = p;
  }
  else if (g_Bpp == 4)
  {
    for (i = 0; i < h; i++)
    {
      i32 = (unsigned int*)(id->pixels +
               ((y + i) * id->lineBytes) + (x * g_Bpp));
      if (i == 0)
      {
        p = *i32;
      }
      for (j = 0; j < w; j++)
      {
        if (i32[j] != p)
        {
          return -1;
        }
      }
    }
    rv = p;
  }
  return rv;
}

/******************************************************************************/
/* split the bitmap up into 64 x 64 pixel areas */
void
rdpup_send_area(struct image_data* id, int x, int y, int w, int h)
{
  char* s;
  int i;
  int single_color;
  int lx;
  int ly;
  int lh;
  int lw;
  int size;
  struct image_data lid;

  LLOGLN(10, ("rdpup_send_area: id %p x %d y %d w %d h %d", id, x, y, w, h));
  if (id == 0)
  {
    rdpup_get_screen_image_rect(&lid);
    id = &lid;
  }

  if (x >= id->width)
  {
    return;
  }
  if (y >= id->height)
  {
    return;
  }
  if (x < 0)
  {
    w += x;
    x = 0;
  }
  if (y < 0)
  {
    h += y;
    y = 0;
  }
  if (w <= 0)
  {
    return;
  }
  if (h <= 0)
  {
    return;
  }
  if (x + w > id->width)
  {
    w = id->width - x;
  }
  if (y + h > id->height)
  {
    h = id->height - y;
  }
  LLOGLN(10, ("%d", w * h));
  if (g_connected && g_begin)
  {
    LLOGLN(10, ("  rdpup_send_area"));
    ly = y;
    while (ly < y + h)
    {
      lx = x;
      while (lx < x + w)
      {
        lw = MIN(64, (x + w) - lx);
        lh = MIN(64, (y + h) - ly);
        single_color = get_single_color(id, lx, ly, lw, lh);
        if (single_color != -1)
        {
          LLOGLN(10, ("%d sending single color", g_count));
          rdpup_set_fgcolor(single_color);
          rdpup_fill_rect(lx, ly, lw, lh);
        }
        else
        {
          size = lw * lh * id->Bpp + 24;
          rdpup_pre_check(size);
          out_uint16_le(g_out_s, 5);
          out_uint16_le(g_out_s, size);
          g_count++;
          out_uint16_le(g_out_s, lx);
          out_uint16_le(g_out_s, ly);
          out_uint16_le(g_out_s, lw);
          out_uint16_le(g_out_s, lh);
          out_uint32_le(g_out_s, lw * lh * id->Bpp);
          for (i = 0; i < lh; i++)
          {
            s = (id->pixels +
                  ((ly + i) * id->lineBytes) + (lx * g_Bpp));
            convert_pixels(s, g_out_s->p, lw);
            g_out_s->p += lw * id->Bpp;
          }
          out_uint16_le(g_out_s, lw);
          out_uint16_le(g_out_s, lh);
          out_uint16_le(g_out_s, 0);
          out_uint16_le(g_out_s, 0);
        }
        lx += 64;
      }
      ly += 64;
    }
  }
}

/******************************************************************************/
void
rdpup_paint_rect_os(int x, int y, int cx, int cy,
                    int rdpindex, int srcx, int srcy)
{
  if (g_connected)
  {
    rdpup_pre_check(20);
    out_uint16_le(g_out_s, 23);
    out_uint16_le(g_out_s, 20);
    g_count++;
    out_uint16_le(g_out_s, x);
    out_uint16_le(g_out_s, y);
    out_uint16_le(g_out_s, cx);
    out_uint16_le(g_out_s, cy);
    out_uint32_le(g_out_s, rdpindex);
    out_uint16_le(g_out_s, srcx);
    out_uint16_le(g_out_s, srcy);
  }
}

/******************************************************************************/
void
rdpup_set_hints(int hints, int mask)
{
  if (g_connected)
  {
    rdpup_pre_check(12);
    out_uint16_le(g_out_s, 24);
    out_uint16_le(g_out_s, 12);
    g_count++;
    out_uint32_le(g_out_s, hints);
    out_uint32_le(g_out_s, mask);
  }
}

/******************************************************************************/
void
rdpup_create_window(WindowPtr pWindow, rdpWindowRec* priv)
{
  int bytes;
  int index;
  int flags;
  int num_window_rects;
  int num_visibility_rects;
  int title_bytes;
  int style;
  int ext_style;
  int root_id;
  char title[256];

  LLOGLN(10, ("rdpup_create_window: id 0x%8.8x", pWindow->drawable.id));
  if (g_connected)
  {
    root_id = pWindow->drawable.pScreen->root->drawable.id;

    if (pWindow->overrideRedirect)
    {
      style = XR_STYLE_TOOLTIP;
      ext_style = XR_EXT_STYLE_TOOLTIP;
    }
    else
    {
      style = XR_STYLE_NORMAL;
      ext_style = XR_EXT_STYLE_NORMAL;
    }

    flags = WINDOW_ORDER_TYPE_WINDOW | WINDOW_ORDER_STATE_NEW;
    strcpy(title, "title");
    title_bytes = strlen(title);

    num_window_rects = 1;
    num_visibility_rects = 1;

    /* calculate bytes */
    bytes = (2 + 2) + (5 * 4) + (2 + title_bytes) + (12 * 4) +
            (2 + num_window_rects * 8) + (4 + 4) +
            (2 + num_visibility_rects * 8) + 4;

    rdpup_pre_check(bytes);
    out_uint16_le(g_out_s, 25);
    out_uint16_le(g_out_s, bytes);
    g_count++;
    out_uint32_le(g_out_s, pWindow->drawable.id); /* window_id */
    out_uint32_le(g_out_s, pWindow->parent->drawable.id); /* owner_window_id */
    flags |= WINDOW_ORDER_FIELD_OWNER;
    out_uint32_le(g_out_s, style); /* style */
    out_uint32_le(g_out_s, ext_style); /* extended_style */
    flags |= WINDOW_ORDER_FIELD_STYLE;
    out_uint32_le(g_out_s, 0); /* show_state */
    flags |= WINDOW_ORDER_FIELD_SHOW;
    out_uint16_le(g_out_s, title_bytes); /* title_info */
    out_uint8a(g_out_s, title, title_bytes);
    flags |= WINDOW_ORDER_FIELD_TITLE;
    out_uint32_le(g_out_s, 0); /* client_offset_x */
    out_uint32_le(g_out_s, 0); /* client_offset_y */
    flags |= WINDOW_ORDER_FIELD_CLIENT_AREA_OFFSET;
    out_uint32_le(g_out_s, pWindow->drawable.width); /* client_area_width */
    out_uint32_le(g_out_s, pWindow->drawable.height); /* client_area_height */
    flags |= WINDOW_ORDER_FIELD_CLIENT_AREA_SIZE;
    out_uint32_le(g_out_s, 0); /* rp_content */
    out_uint32_le(g_out_s, root_id); /* root_parent_handle */
    flags |= WINDOW_ORDER_FIELD_ROOT_PARENT;
    out_uint32_le(g_out_s, pWindow->drawable.x); /* window_offset_x */
    out_uint32_le(g_out_s, pWindow->drawable.y); /* window_offset_y */
    flags |= WINDOW_ORDER_FIELD_WND_OFFSET;
    out_uint32_le(g_out_s, 0); /* window_client_delta_x */
    out_uint32_le(g_out_s, 0); /* window_client_delta_y */
    flags |= WINDOW_ORDER_FIELD_WND_CLIENT_DELTA;
    out_uint32_le(g_out_s, pWindow->drawable.width); /* window_width */
    out_uint32_le(g_out_s, pWindow->drawable.height); /* window_height */
    flags |= WINDOW_ORDER_FIELD_WND_SIZE;
    out_uint16_le(g_out_s, num_window_rects); /* num_window_rects */
    for (index = 0; index < num_window_rects; index++)
    {
      out_uint16_le(g_out_s, 0); /* left */
      out_uint16_le(g_out_s, 0); /* top */
      out_uint16_le(g_out_s, pWindow->drawable.width); /* right */
      out_uint16_le(g_out_s, pWindow->drawable.height); /* bottom */
    }
    flags |= WINDOW_ORDER_FIELD_WND_RECTS;
    out_uint32_le(g_out_s, pWindow->drawable.x); /* visible_offset_x */
    out_uint32_le(g_out_s, pWindow->drawable.y); /* visible_offset_y */
    flags |= WINDOW_ORDER_FIELD_VIS_OFFSET;
    out_uint16_le(g_out_s, num_visibility_rects); /* num_visibility_rects */
    for (index = 0; index < num_visibility_rects; index++)
    {
      out_uint16_le(g_out_s, 0); /* left */
      out_uint16_le(g_out_s, 0); /* top */
      out_uint16_le(g_out_s, pWindow->drawable.width); /* right */
      out_uint16_le(g_out_s, pWindow->drawable.height); /* bottom */
    }
    flags |= WINDOW_ORDER_FIELD_VISIBILITY;

    out_uint32_le(g_out_s, flags); /* flags */
  }
}

/******************************************************************************/
void
rdpup_delete_window(WindowPtr pWindow, rdpWindowRec* priv)
{
  LLOGLN(10, ("rdpup_delete_window: id 0x%8.8x", pWindow->drawable.id));
  if (g_connected)
  {
    rdpup_pre_check(8);
    out_uint16_le(g_out_s, 26);
    out_uint16_le(g_out_s, 8);
    g_count++;
    out_uint32_le(g_out_s, pWindow->drawable.id); /* window_id */
  }
}

/******************************************************************************/
int
rdpup_check_dirty(PixmapPtr pDirtyPixmap, rdpPixmapRec* pDirtyPriv)
{
  int index;
  int clip_index;
  int count;
  int num_clips;
  BoxRec box;
  xSegment* seg;
  struct image_data id;
  struct rdp_draw_item* di;

  if (pDirtyPriv == 0)
  {
    return 0;
  }
  if (pDirtyPriv->is_dirty == 0)
  {
    return 0;
  }

  /* update use time / count */
  g_os_bitmaps[pDirtyPriv->rdpindex].stamp = g_os_bitmap_stamp;
  g_os_bitmap_stamp++;

  LLOGLN(10, ("-----------------got dirty"));
  rdpup_switch_os_surface(pDirtyPriv->rdpindex);
  rdpup_get_pixmap_image_rect(pDirtyPixmap, &id);
  rdpup_begin_update();
  draw_item_pack(pDirtyPriv);
  di = pDirtyPriv->draw_item_head;
  while (di != 0)
  {
    LLOGLN(10, ("rdpup_check_dirty: type %d", di->type));
    switch (di->type)
    {
      case RDI_FILL:
        rdpup_set_fgcolor(di->u.fill.fg_color);
        rdpup_set_opcode(di->u.fill.opcode);
        count = REGION_NUM_RECTS(di->reg);
        for (index = 0; index < count; index++)
        {
          box = REGION_RECTS(di->reg)[index];
          LLOGLN(10, ("  RDI_FILL %d %d %d %d", box.x1, box.y1,
                 box.x2, box.y2));
          rdpup_fill_rect(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
        }
        rdpup_set_opcode(GXcopy);
        break;
      case RDI_IMGLL:
        rdpup_set_hints(1, 1);
        rdpup_set_opcode(di->u.img.opcode);
        count = REGION_NUM_RECTS(di->reg);
        for (index = 0; index < count; index++)
        {
          box = REGION_RECTS(di->reg)[index];
          LLOGLN(10, ("  RDI_IMGLL %d %d %d %d", box.x1, box.y1,
                 box.x2, box.y2));
          rdpup_send_area(&id, box.x1, box.y1, box.x2 - box.x1,
                          box.y2 - box.y1);
        }
        rdpup_set_opcode(GXcopy);
        rdpup_set_hints(0, 1);
        break;
      case RDI_IMGLY:
        rdpup_set_opcode(di->u.img.opcode);
        count = REGION_NUM_RECTS(di->reg);
        for (index = 0; index < count; index++)
        {
          box = REGION_RECTS(di->reg)[index];
          LLOGLN(10, ("  RDI_IMGLY %d %d %d %d", box.x1, box.y1,
                 box.x2, box.y2));
          rdpup_send_area(&id, box.x1, box.y1, box.x2 - box.x1,
                          box.y2 - box.y1);
        }
        rdpup_set_opcode(GXcopy);
        break;
      case RDI_LINE:
        LLOGLN(10, ("  RDI_LINE"));
        num_clips = REGION_NUM_RECTS(di->reg);
        if (num_clips > 0)
        {
          rdpup_set_fgcolor(di->u.line.fg_color);
          rdpup_set_opcode(di->u.line.opcode);
          rdpup_set_pen(0, di->u.line.width);
          for (clip_index = num_clips - 1; clip_index >= 0; clip_index--)
          {
            box = REGION_RECTS(di->reg)[clip_index];
            rdpup_set_clip(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
            for (index = 0; index < di->u.line.nseg; index++)
            {
              seg = di->u.line.segs + index;
              LLOGLN(10, ("  RDI_LINE %d %d %d %d", seg->x1, seg->y1,
                     seg->x2, seg->y2));
              rdpup_draw_line(seg->x1, seg->y1, seg->x2, seg->y2);
            }
          }
        }
        rdpup_reset_clip();
        rdpup_set_opcode(GXcopy);
        break;
      case RDI_SCRBLT:
        LLOGLN(10, ("  RDI_SCRBLT"));
        break;
    }
    di = di->next;
  }
  draw_item_remove_all(pDirtyPriv);
  rdpup_end_update();
  pDirtyPriv->is_dirty = 0;
  rdpup_switch_os_surface(-1);
  return 0;
}

/******************************************************************************/
int
rdpup_check_dirty_screen(rdpPixmapRec* pDirtyPriv)
{
  int index;
  int clip_index;
  int count;
  int num_clips;
  BoxRec box;
  xSegment* seg;
  struct image_data id;
  struct rdp_draw_item* di;

  if (pDirtyPriv == 0)
  {
    return 0;
  }
  if (pDirtyPriv->is_dirty == 0)
  {
    return 0;
  }

  LLOGLN(10, ("-----------------got dirty"));
  rdpup_get_screen_image_rect(&id);
  rdpup_begin_update();
  draw_item_pack(pDirtyPriv);
  di = pDirtyPriv->draw_item_head;
  while (di != 0)
  {
    LLOGLN(10, ("rdpup_check_dirty_screen: type %d", di->type));
    switch (di->type)
    {
      case RDI_FILL:
        rdpup_set_fgcolor(di->u.fill.fg_color);
        rdpup_set_opcode(di->u.fill.opcode);
        count = REGION_NUM_RECTS(di->reg);
        for (index = 0; index < count; index++)
        {
          box = REGION_RECTS(di->reg)[index];
          LLOGLN(10, ("  RDI_FILL %d %d %d %d", box.x1, box.y1,
                 box.x2, box.y2));
          rdpup_fill_rect(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
        }
        rdpup_set_opcode(GXcopy);
        break;
      case RDI_IMGLL:
        rdpup_set_hints(1, 1);
        rdpup_set_opcode(di->u.img.opcode);
        count = REGION_NUM_RECTS(di->reg);
        for (index = 0; index < count; index++)
        {
          box = REGION_RECTS(di->reg)[index];
          LLOGLN(10, ("  RDI_IMGLL %d %d %d %d", box.x1, box.y1,
                 box.x2, box.y2));
          rdpup_send_area(&id, box.x1, box.y1, box.x2 - box.x1,
                          box.y2 - box.y1);
        }
        rdpup_set_opcode(GXcopy);
        rdpup_set_hints(0, 1);
        break;
      case RDI_IMGLY:
        rdpup_set_opcode(di->u.img.opcode);
        count = REGION_NUM_RECTS(di->reg);
        for (index = 0; index < count; index++)
        {
          box = REGION_RECTS(di->reg)[index];
          LLOGLN(10, ("  RDI_IMGLY %d %d %d %d", box.x1, box.y1,
                 box.x2, box.y2));
          rdpup_send_area(&id, box.x1, box.y1, box.x2 - box.x1,
                          box.y2 - box.y1);
        }
        rdpup_set_opcode(GXcopy);
        break;
      case RDI_LINE:
        LLOGLN(10, ("  RDI_LINE"));
        num_clips = REGION_NUM_RECTS(di->reg);
        if (num_clips > 0)
        {
          rdpup_set_fgcolor(di->u.line.fg_color);
          rdpup_set_opcode(di->u.line.opcode);
          rdpup_set_pen(0, di->u.line.width);
          for (clip_index = num_clips - 1; clip_index >= 0; clip_index--)
          {
            box = REGION_RECTS(di->reg)[clip_index];
            rdpup_set_clip(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
            for (index = 0; index < di->u.line.nseg; index++)
            {
              seg = di->u.line.segs + index;
              LLOGLN(10, ("  RDI_LINE %d %d %d %d", seg->x1, seg->y1,
                     seg->x2, seg->y2));
              rdpup_draw_line(seg->x1, seg->y1, seg->x2, seg->y2);
            }
          }
        }
        rdpup_reset_clip();
        rdpup_set_opcode(GXcopy);
        break;
      case RDI_SCRBLT:
        LLOGLN(10, ("  RDI_SCRBLT"));
        break;
    }
    di = di->next;
  }
  draw_item_remove_all(pDirtyPriv);
  rdpup_end_update();
  pDirtyPriv->is_dirty = 0;
  return 0;
}
