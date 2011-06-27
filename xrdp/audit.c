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

   Copyright (C) Osirium Ltd 2010

*/
#include "xrdp.h"
#include "stdio.h"
#define AUDIT_ADDRESS "127.0.0.1"
#define AUDIT_PORT  "8081"

#define REQUEST_TEMPLATE "GET /uitask/user/%s/connection/%s/device/%s/%s/%s?ip_address=%s\r\n\r\n"

int APP_CC
xrdp_audit(struct xrdp_process *process, const char*action, const char* message)
{
    // This sends an http 0.9 request, i.e. no headers
    char client_ip[256] = {0,};
    char data[4096] = {0,};
    char username[256] = {0,};
    char device_name[256] = {0,};
    char accesstoken[256] = {0,};
    xrdp_mm_get_value(process->wm->mm, "osirium_account", username, 255);
    xrdp_mm_get_value(process->wm->mm, "device_name", device_name, 255);
    xrdp_mm_get_value(process->wm->mm, "accesstoken", accesstoken, 255);
    xrdp_mm_get_value(process->wm->mm, "client_ip_addr", client_ip, 255);
    if (username[0] == 0)
    {
        g_snprintf(username, 255, "%s", g_getenv("USER"));
    }
    if (device_name[0] == 0)
    {
        g_strcpy(device_name, "unknown");
    }
    if (accesstoken[0] == 0)
    {
        g_strcpy(accesstoken, "unknown");
    }

    g_snprintf(data, sizeof(data)-1, REQUEST_TEMPLATE, 
            username, //process->wm->session->client_info->username,// username
            "rdp",                                      // type
            device_name,    //process->session->client_info->hostname,    // devicename
            action,                                     // action
            accesstoken,                                         // accesstoken
            // process->server_trans->skt
            client_ip //process->session->client_info->hostname // client_ip
            );
    g_writeln(data);
    fflush(stdout);

    // open socket
    int sck = g_tcp_socket();
    fflush(stdout);
    if (g_tcp_connect(sck, AUDIT_ADDRESS, AUDIT_PORT) == 0)
    {
        // left as blocking socket !!
        fflush(stdout);
        // get url
        int sent = g_tcp_send(sck, data, g_strlen(data), 0);
        if ( g_tcp_can_recv(sck, 1000) > 0)  // at most 1 second
        {
            // read response and ignore.
            int rlen = g_tcp_recv(sck, data, sizeof(data)-1, 0);
        }
        else
        {
        }
        fflush(stdout);
        // close socket
        g_tcp_close(sck);
    }
}

