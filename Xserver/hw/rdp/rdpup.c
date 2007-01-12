/*
Copyright 2005-2007 Jay Sorg

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
/*#include "arch.h"*/
/*#include "parse.h"*/
/*#include "os_calls.h"*/

#define DEBUG_OUT_UP(arg)
/*#define DEBUG_OUT_UP(arg) ErrorF arg*/

static int g_listen_sck = 0;
static int g_sck = 0;
static int g_sck_closed = 0;
static int g_connected = 0;
static int g_begin = 0;
static struct stream* g_out_s = 0;
static struct stream* g_in_s = 0;
static int g_button_mask = 0;
static int g_cursor_x = 0;
static int g_cursor_y = 0;
static OsTimerPtr g_timer = 0;
static int g_scheduled = 0;
static int g_count = 0;

extern ScreenPtr g_pScreen; /* from rdpmain.c */
extern int g_Bpp; /* from rdpmain.c */
extern int g_Bpp_mask; /* from rdpmain.c */
extern rdpScreenInfo g_rdpScreen; /* from rdpmain.c */

extern char* display;

static void
rdpScheduleDeferredUpdate(void);

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

static int rdp_opcodes[16] =
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
        RemoveEnabledDevice(g_sck);
        g_connected = 0;
        g_tcp_close(g_sck);
        g_sck = 0;
        g_sck_closed = 1;
        return 1;
      }
    }
    else if (rcvd == 0)
    {
      RemoveEnabledDevice(g_sck);
      g_connected = 0;
      g_tcp_close(g_sck);
      g_sck = 0;
      g_sck_closed = 1;
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

/*****************************************************************************/
/* returns error */
static int
rdpup_send(char* data, int len)
{
  int sent;

  DEBUG_OUT_UP(("rdpup_send - sending %d bytes\n", len));
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
        RemoveEnabledDevice(g_sck);
        g_connected = 0;
        g_tcp_close(g_sck);
        g_sck = 0;
        g_sck_closed = 1;
        return 1;
      }
    }
    else if (sent == 0)
    {
      RemoveEnabledDevice(g_sck);
      g_connected = 0;
      g_tcp_close(g_sck);
      g_sck = 0;
      g_sck_closed = 1;
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
    len = s->end - s->data;
    if (len > s->size)
    {
      ErrorF("overrun error len %d count %d\n", len, g_count);
    }
    s_pop_layer(s, iso_hdr);
    out_uint16_le(s, 1);
    out_uint16_le(s, g_count);
    out_uint32_le(s, len - 8);
    rv = rdpup_send(s->data, len);
  }
  if (rv != 0)
  {
    ErrorF("error in rdpup_send_msg\n");
  }
  return rv;
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
    ErrorF("error in rdpup_recv_msg\n");
  }
  return rv;
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

  in_uint16_le(s, msg_type);
  if (msg_type == 103)
  {
    in_uint32_le(s, msg);
    in_uint32_le(s, param1);
    in_uint32_le(s, param2);
    in_uint32_le(s, param3);
    in_uint32_le(s, param4);
    DEBUG_OUT_UP(("rdpup_process_msg - msg %d param1 %d param2 %d param3 %d \
param4 %d\n", msg, param1, param2, param3, param4));
    /*ErrorF("rdpup_process_msg - msg %d param1 %d param2 %d param3 %d \
param4 %d\n", msg, param1, param2, param3, param4);*/
    switch (msg)
    {
      case 15: /* key down */
      case 16: /* key up */
        KbdAddEvent(msg == 15, param1, param2, param3, param4);
        break;
      case 17: /* from RDP_INPUT_SYNCHRONIZE */
#if 0
        /* scroll lock */
        if (param1 & 1)
        {
          KbdAddEvent(1, 70, 0, 70, 0);
        }
        else
        {
          KbdAddEvent(0, 70, 49152, 70, 49152);
        }
        /* num lock */
        if (param1 & 2)
        {
          KbdAddEvent(1, 69, 0, 69, 0);
        }
        else
        {
          KbdAddEvent(0, 69, 49152, 69, 49152);
        }
        /* caps lock */
        if (param1 & 4)
        {
          KbdAddEvent(1, 58, 0, 58, 0);
        }
        else
        {
          KbdAddEvent(0, 58, 49152, 58, 49152);
        }
#endif
        break;
      case 100:
        g_cursor_x = param1;
        g_cursor_y = param2;
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
        rdpup_send_area((param1 >> 16) & 0xffff, param1 & 0xffff,
                        (param2 >> 16) & 0xffff, param2 & 0xffff);
        rdpup_end_update();
        break;
    }
  }
  else
  {
    ErrorF("unknown message type in rdpup_process_msg\n");
  }
  return 0;
}

/******************************************************************************/
int
rdpup_init(void)
{
  char text[256];
  int i;

  i = atoi(display);
  if (i < 1)
  {
    return 0;
  }
  g_sprintf(text, "62%2.2d", i);
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
  return 1;
}

/******************************************************************************/
int
rdpup_check(void)
{
  int sel;

  sel = g_tcp_select(g_listen_sck, g_sck);
  if (sel & 1)
  {
    if (g_sck == 0)
    {
      g_sck = g_tcp_accept(g_listen_sck);
      if (g_sck == -1)
      {
        g_sck = 0;
      }
      else
      {
        g_tcp_set_non_blocking(g_sck);
        g_tcp_set_no_delay(g_sck);
        g_connected = 1;
        g_sck_closed = 0;
        AddEnabledDevice(g_sck);
      }
    }
    else
    {
      ErrorF("rejecting connection\n");
      g_sleep(10);
      g_tcp_close(g_tcp_accept(g_listen_sck));
    }
  }
  if (sel & 2)
  {
    if (rdpup_recv_msg(g_in_s) == 0)
    {
      rdpup_process_msg(g_in_s);
    }
  }
  return 0;
}

/******************************************************************************/
int
rdpup_begin_update(void)
{
  if (g_connected)
  {
    if (g_begin)
    {
      return 0;
    }
    init_stream(g_out_s, 0);
    s_push_layer(g_out_s, iso_hdr, 8);
    out_uint16_le(g_out_s, 1);
    DEBUG_OUT_UP(("begin %d\n", g_count));
    g_begin = 1;
    g_count = 1;
  }
  return 0;
}

/******************************************************************************/
int
rdpup_end_update(void)
{
  if (g_connected && g_begin)
  {
    rdpScheduleDeferredUpdate();
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
    /*ErrorF("%d %d\n", in_size, g_out_s->size);*/
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
    DEBUG_OUT_UP(("  rdpup_fill_rect\n"));
    rdpup_pre_check(10);
    out_uint16_le(g_out_s, 3);
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
    DEBUG_OUT_UP(("  rdpup_screen_blt\n"));
    rdpup_pre_check(14);
    out_uint16_le(g_out_s, 4);
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
    DEBUG_OUT_UP(("  rdpup_set_clip\n"));
    rdpup_pre_check(10);
    out_uint16_le(g_out_s, 10);
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
    DEBUG_OUT_UP(("  rdpup_reset_clip\n"));
    rdpup_pre_check(2);
    out_uint16_le(g_out_s, 11);
    g_count++;
  }
  return 0;
}

/******************************************************************************/
int
rdpup_set_fgcolor(int fgcolor)
{
  if (g_connected)
  {
    DEBUG_OUT_UP(("  rdpup_set_fgcolor\n"));
    rdpup_pre_check(6);
    out_uint16_le(g_out_s, 12);
    g_count++;
    fgcolor = fgcolor & g_Bpp_mask;
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
    DEBUG_OUT_UP(("  rdpup_set_bgcolor\n"));
    rdpup_pre_check(6);
    out_uint16_le(g_out_s, 13);
    g_count++;
    bgcolor = bgcolor & g_Bpp_mask;
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
    DEBUG_OUT_UP(("  rdpup_set_opcode\n"));
    rdpup_pre_check(4);
    out_uint16_le(g_out_s, 14);
    g_count++;
    out_uint16_le(g_out_s, rdp_opcodes[opcode & 0xf]);
  }
  return 0;
}

/******************************************************************************/
int
rdpup_set_pen(int style, int width)
{
  if (g_connected)
  {
    DEBUG_OUT_UP(("  rdpup_set_pen\n"));
    rdpup_pre_check(6);
    out_uint16_le(g_out_s, 17);
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
    DEBUG_OUT_UP(("  rdpup_draw_line\n"));
    rdpup_pre_check(10);
    out_uint16_le(g_out_s, 18);
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
  if (g_connected)
  {
    DEBUG_OUT_UP(("  rdpup_set_cursor\n"));
    rdpup_pre_check(6 + 32 * (32 * 3) + 32 * (32 / 8));
    out_uint16_le(g_out_s, 19);
    g_count++;
    out_uint16_le(g_out_s, x);
    out_uint16_le(g_out_s, y);
    out_uint8a(g_out_s, cur_data, 32 * (32 * 3));
    out_uint8a(g_out_s, cur_mask, 32 * (32 / 8));
  }
  return 0;
}

/******************************************************************************/
static int
get_single_color(int x, int y, int w, int h)
{
  int rv;
  int i;
  int j;
  int p;
  unsigned char* i8;
  unsigned short* i16;

  rv = -1;
  if (g_Bpp == 1)
  {
    for (i = 0; i < h; i++)
    {
      i8 = (unsigned char*)(g_rdpScreen.pfbMemory +
               ((y + i) * g_rdpScreen.paddedWidthInBytes) + (x * g_Bpp));
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
      i16 = (unsigned short*)(g_rdpScreen.pfbMemory +
               ((y + i) * g_rdpScreen.paddedWidthInBytes) + (x * g_Bpp));
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
  return rv;
}

/******************************************************************************/
/* split the bitmap up into 64 x 64 pixel areas */
void
rdpup_send_area(int x, int y, int w, int h)
{
  char* s;
  int i;
  int single_color;
  int lx;
  int ly;
  int lh;
  int lw;

  if (x >= g_rdpScreen.width)
  {
    return;
  }
  if (y >= g_rdpScreen.height)
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
  if (x + w > g_rdpScreen.width)
  {
    w = g_rdpScreen.width - x;
  }
  if (y + h > g_rdpScreen.height)
  {
    h = g_rdpScreen.height - y;
  }
  /*ErrorF("%d\n", w * h);*/
  if (g_connected && g_begin)
  {
    DEBUG_OUT_UP(("  rdpup_send_area\n"));
    ly = y;
    while (ly < y + h)
    {
      lx = x;
      while (lx < x + w)
      {
        lw = MIN(64, (x + w) - lx);
        lh = MIN(64, (y + h) - ly);
        single_color = get_single_color(lx, ly, lw, lh);
        if (single_color != -1)
        {
          /*ErrorF("%d sending single color\n", g_count);*/
          rdpup_set_fgcolor(single_color);
          rdpup_fill_rect(lx, ly, lw, lh);
        }
        else
        {
          rdpup_pre_check(lw * lh * g_Bpp + 42);
          out_uint16_le(g_out_s, 5);
          g_count++;
          out_uint16_le(g_out_s, lx);
          out_uint16_le(g_out_s, ly);
          out_uint16_le(g_out_s, lw);
          out_uint16_le(g_out_s, lh);
          out_uint32_le(g_out_s, lw * lh * g_Bpp);
          for (i = 0; i < lh; i++)
          {
            s = (g_rdpScreen.pfbMemory +
                  ((ly + i) * g_rdpScreen.paddedWidthInBytes) + (lx * g_Bpp));
            out_uint8a(g_out_s, s, lw * g_Bpp);
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
static CARD32
rdpDeferredUpdateCallback(OsTimerPtr timer, CARD32 now, pointer arg)
{
  if (g_connected && g_begin)
  {
    DEBUG_OUT_UP(("end %d\n", g_count));
    out_uint16_le(g_out_s, 2);
    g_count++;
    s_mark_end(g_out_s);
    rdpup_send_msg(g_out_s);
  }
  g_count = 0;
  g_begin = 0;
  g_scheduled = 0;
  return 0;
}

/******************************************************************************/
static void
rdpScheduleDeferredUpdate(void)
{
  if (!g_scheduled)
  {
    g_scheduled = 1;
    g_timer = TimerSet(g_timer, 0, 40, rdpDeferredUpdateCallback, 0);
  }
}
