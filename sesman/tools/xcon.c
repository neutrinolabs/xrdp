
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>

Display* g_display = 0;
int g_x_socket = 0;

int main(int argc, char** argv)
{
  fd_set rfds;
  int i1;
  XEvent xevent;

  g_display = XOpenDisplay(0);
  if (g_display == 0)
  {
    printf("XOpenDisplay failed\n");
    return 0;
  }
  g_x_socket = XConnectionNumber(g_display);
  while (1)
  {
    FD_ZERO(&rfds);
    FD_SET(g_x_socket, &rfds);
    i1 = select(g_x_socket + 1, &rfds, 0, 0, 0);
    if (i1 < 0)
    {
      break;
    }
    XNextEvent(g_display, &xevent);
  }
  return 0;
}
