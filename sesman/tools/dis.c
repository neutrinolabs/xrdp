/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   xrdp: A Remote Desktop Protocol server.
   Copyright (C) Jay Sorg 2011
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

int main(int argc, char** argv)
{
  int sck;
  int dis;
  struct sockaddr_un sa;
  size_t len;
  char* p;
  char* display;

  if (argc != 1)
  {
    printf("xrdp disconnect utility\n");
    printf("run with no parameters to disconnect you xrdp session\n");
    return 0;
  }

  display = getenv("DISPLAY");
  if (display == 0)
  {
    printf("display not set\n");
    return 1;
  }
  dis = strtol(display + 1, &p, 10);
  memset(&sa, 0, sizeof(sa));
  sa.sun_family = AF_UNIX;
  sprintf(sa.sun_path, "/tmp/.xrdp/xrdp_disconnect_display_%d", dis);
  if (access(sa.sun_path, F_OK) != 0)
  {
    printf("not in an xrdp session\n");
    return 1;
  }
  sck = socket(PF_UNIX, SOCK_DGRAM, 0);
  len = sizeof(sa);
  if (sendto(sck, "sig", 4, 0, (struct sockaddr*)&sa, len) > 0)
  {
    printf("message sent ok\n");
  }
  return 0;
}
