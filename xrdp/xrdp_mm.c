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
   Copyright (C) Jay Sorg 2004-2010

   module manager

*/

#include "xrdp.h"

/*****************************************************************************/
struct xrdp_mm* APP_CC
xrdp_mm_create(struct xrdp_wm* owner)
{
  struct xrdp_mm* self;

  self = (struct xrdp_mm*)g_malloc(sizeof(struct xrdp_mm), 1);
  self->wm = owner;
  self->login_names = list_create();
  self->login_names->auto_free = 1;
  self->login_values = list_create();
  self->login_values->auto_free = 1;
  return self;
}

/*****************************************************************************/
/* called from main thread */
static long DEFAULT_CC
xrdp_mm_sync_unload(long param1, long param2)
{
  return g_free_library(param1);
}

/*****************************************************************************/
/* called from main thread */
static long DEFAULT_CC
xrdp_mm_sync_load(long param1, long param2)
{
  long rv;
  char* libname;

  libname = (char*)param1;
  rv = g_load_library(libname);
  return rv;
}

/*****************************************************************************/
static void APP_CC
xrdp_mm_module_cleanup(struct xrdp_mm* self)
{
  g_writeln("xrdp_mm_module_cleanup");
  if (self->mod != 0)
  {
    if (self->mod_exit != 0)
    {
      /* let the module cleanup */
      self->mod_exit(self->mod);
    }
  }
  if (self->mod_handle != 0)
  {
    /* Let the main thread unload the module.*/
    g_xrdp_sync(xrdp_mm_sync_unload, self->mod_handle, 0);
  }
  trans_delete(self->chan_trans);
  self->chan_trans = 0;
  self->chan_trans_up = 0;
  self->mod_init = 0;
  self->mod_exit = 0;
  self->mod = 0;
  self->mod_handle = 0;
}

/*****************************************************************************/
void APP_CC
xrdp_mm_delete(struct xrdp_mm* self)
{
  if (self == 0)
  {
    return;
  }
  /* free any module stuff */
  xrdp_mm_module_cleanup(self);
  trans_delete(self->sesman_trans);
  self->sesman_trans = 0;
  self->sesman_trans_up = 0;
  list_delete(self->login_names);
  list_delete(self->login_values);
  g_free(self);
}

/*****************************************************************************/
static int APP_CC
xrdp_mm_send_login(struct xrdp_mm* self)
{
  struct stream* s;
  int rv;
  int index;
  int count;
  int xserverbpp;
  char* username;
  char* password;
  char* name;
  char* value;

  xrdp_wm_log_msg(self->wm, "sending login info to session manager, "
                            "please wait...");
  username = 0;
  password = 0;
  self->code = 0;
  xserverbpp = 0;
  count = self->login_names->count;
  for (index = 0; index < count; index++)
  {
    name = (char*)list_get_item(self->login_names, index);
    value = (char*)list_get_item(self->login_values, index);
    if (g_strcasecmp(name, "username") == 0)
    {
      username = value;
    }
    else if (g_strcasecmp(name, "password") == 0)
    {
      password = value;
    }
    else if (g_strcasecmp(name, "lib") == 0)
    {
      if ((g_strcasecmp(value, "libxup.so") == 0) ||
          (g_strcasecmp(value, "xup.dll") == 0))
      {
        self->code = 10;
      }
    }
    else if (g_strcasecmp(name, "xserverbpp") == 0)
    {
      xserverbpp = g_atoi(value);
    }
  }
  if ((username == 0) || (password == 0))
  {
    xrdp_wm_log_msg(self->wm, "Error finding username and password");
    return 1;
  }

  s = trans_get_out_s(self->sesman_trans, 8192);
  s_push_layer(s, channel_hdr, 8);
  /* this code is either 0 for Xvnc or 10 for X11rdp */
  out_uint16_be(s, self->code);
  index = g_strlen(username);
  out_uint16_be(s, index);
  out_uint8a(s, username, index);
  index = g_strlen(password);

  out_uint16_be(s, index);
  out_uint8a(s, password, index);
  out_uint16_be(s, self->wm->screen->width);
  out_uint16_be(s, self->wm->screen->height);

  if (xserverbpp > 0)
  {
    out_uint16_be(s, xserverbpp);
  }
  else
  {
    out_uint16_be(s, self->wm->screen->bpp);
  }

  /* send domain */
  index = g_strlen(self->wm->client_info->domain);
  out_uint16_be(s, index);
  out_uint8a(s, self->wm->client_info->domain, index);

  /* send program / shell */
  index = g_strlen(self->wm->client_info->program);
  out_uint16_be(s, index);
  out_uint8a(s, self->wm->client_info->program, index);

  /* send directory */
  index = g_strlen(self->wm->client_info->directory);
  out_uint16_be(s, index);
  out_uint8a(s, self->wm->client_info->directory, index);

  /* send client ip */
  index = g_strlen(self->wm->client_info->client_ip);
  out_uint16_be(s, index);
  out_uint8a(s, self->wm->client_info->client_ip, index);

  s_mark_end(s);

  s_pop_layer(s, channel_hdr);
  out_uint32_be(s, 0); /* version */
  index = (int)(s->end - s->data);
  out_uint32_be(s, index); /* size */

  rv = trans_force_write(self->sesman_trans);

  if (rv != 0) {
    xrdp_wm_log_msg(self->wm, "xrdp_mm_send_login: xrdp_mm_send_login failed");
  }

  return rv;
}

/*****************************************************************************/
/* returns error */
/* this goes through the login_names looking for one called 'aname'
   then it copies the corisponding login_values item into 'dest'
   'dest' must be at least 'dest_len' + 1 bytes in size */
static int APP_CC
xrdp_mm_get_value(struct xrdp_mm* self, char* aname, char* dest, int dest_len)
{
  char* name;
  char* value;
  int index;
  int count;
  int rv;

  rv = 1;
  /* find the library name */
  dest[0] = 0;
  count = self->login_names->count;
  for (index = 0; index < count; index++)
  {
    name = (char*)list_get_item(self->login_names, index);
    value = (char*)list_get_item(self->login_values, index);
    if ((name == 0) || (value == 0))
    {
      break;
    }
    if (g_strcasecmp(name, aname) == 0)
    {
      g_strncpy(dest, value, dest_len);
      rv = 0;
    }
  }

  return rv;
}

/*****************************************************************************/
static int APP_CC
xrdp_mm_setup_mod1(struct xrdp_mm* self)
{
  void* func;
  char lib[256];
  char text[256];

  if (self == 0)
  {
    return 1;
  }
  lib[0] = 0;
  if (xrdp_mm_get_value(self, "lib", lib, 255) != 0)
  {
    g_snprintf(text, 255, "no library name specified in xrdp.ini, please add "
                          "lib=libxrdp-vnc.so or similar");
    xrdp_wm_log_msg(self->wm, text);

    return 1;
  }
  if (lib[0] == 0)
  {
    g_snprintf(text, 255, "empty library name specified in xrdp.ini, please "
                          "add lib=libxrdp-vnc.so or similar");
    xrdp_wm_log_msg(self->wm, text);

    return 1;
  }
  if (self->mod_handle == 0)
  {
    /* Let the main thread load the lib,*/  
    self->mod_handle = g_xrdp_sync(xrdp_mm_sync_load, (long)lib, 0);
    if (self->mod_handle != 0)
    {
      func = g_get_proc_address(self->mod_handle, "mod_init");
      if (func == 0)
      {
        func = g_get_proc_address(self->mod_handle, "_mod_init");
      }
      if (func == 0)
      {
        g_snprintf(text, 255, "error finding proc mod_init in %s, not a valid "
                              "xrdp backend", lib);
        xrdp_wm_log_msg(self->wm, text);
      }
      self->mod_init = (struct xrdp_mod* (*)(void))func;
      func = g_get_proc_address(self->mod_handle, "mod_exit");
      if (func == 0)
      {
        func = g_get_proc_address(self->mod_handle, "_mod_exit");
      }
      if (func == 0)
      {
        g_snprintf(text, 255, "error finding proc mod_exit in %s, not a valid "
                              "xrdp backend", lib);
        xrdp_wm_log_msg(self->wm, text);
      }
      self->mod_exit = (int (*)(struct xrdp_mod*))func;
      if ((self->mod_init != 0) && (self->mod_exit != 0))
      {
        self->mod = self->mod_init();
        if (self->mod != 0)
        {
          g_writeln("loaded module '%s' ok, interface size %d, version %d", lib,
                    self->mod->size, self->mod->version);
        }
      }
    }
    else
    {
      g_snprintf(text, 255, "error loading %s specified in xrdp.ini, please "
                            "add a valid entry like lib=libxrdp-vnc.so or similar", lib);
      xrdp_wm_log_msg(self->wm, text);
    }
    if (self->mod != 0)
    {
      self->mod->wm = (long)(self->wm);
      self->mod->server_begin_update = server_begin_update;
      self->mod->server_end_update = server_end_update;
      self->mod->server_bell_trigger = server_bell_trigger;
      self->mod->server_fill_rect = server_fill_rect;
      self->mod->server_screen_blt = server_screen_blt;
      self->mod->server_paint_rect = server_paint_rect;
      self->mod->server_set_pointer = server_set_pointer;
      self->mod->server_palette = server_palette;
      self->mod->server_msg = server_msg;
      self->mod->server_is_term = server_is_term;
      self->mod->server_set_clip = server_set_clip;
      self->mod->server_reset_clip = server_reset_clip;
      self->mod->server_set_fgcolor = server_set_fgcolor;
      self->mod->server_set_bgcolor = server_set_bgcolor;
      self->mod->server_set_opcode = server_set_opcode;
      self->mod->server_set_mixmode = server_set_mixmode;
      self->mod->server_set_brush = server_set_brush;
      self->mod->server_set_pen = server_set_pen;
      self->mod->server_draw_line = server_draw_line;
      self->mod->server_add_char = server_add_char;
      self->mod->server_draw_text = server_draw_text;
      self->mod->server_reset = server_reset;
      self->mod->server_query_channel = server_query_channel;
      self->mod->server_get_channel_id = server_get_channel_id;
      self->mod->server_send_to_channel = server_send_to_channel;
      self->mod->server_create_os_surface = server_create_os_surface;
      self->mod->server_switch_os_surface = server_switch_os_surface;
      self->mod->server_delete_os_surface = server_delete_os_surface;
      self->mod->server_paint_rect_os = server_paint_rect_os;
      self->mod->server_set_hints = server_set_hints;
    }
  }
  /* id self->mod is null, there must be a problem */
  if (self->mod == 0)
  {
    DEBUG(("problem loading lib in xrdp_mm_setup_mod1"));
    return 1;
  }
  return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_mm_setup_mod2(struct xrdp_mm* self)
{
  char text[256];
  char* name;
  char* value;
  int i;
  int rv;
  int key_flags;
  int device_flags;
  int use_uds;

  rv = 1; /* failure */
  g_memset(text, 0, sizeof(text));
  if (!g_is_wait_obj_set(self->wm->pro_layer->self_term_event))
  {
    if (self->mod->mod_start(self->mod, self->wm->screen->width,
                             self->wm->screen->height,
                             self->wm->screen->bpp) != 0)
    {
      g_set_wait_obj(self->wm->pro_layer->self_term_event); /* kill session */
    }
  }
  if (!g_is_wait_obj_set(self->wm->pro_layer->self_term_event))
  {
    if (self->display > 0)
    {
      if (self->code == 0) /* Xvnc */
      {
        g_snprintf(text, 255, "%d", 5900 + self->display);
      }
      else if (self->code == 10) /* X11rdp */
      {
        use_uds = 1;
        if (xrdp_mm_get_value(self, "ip", text, 255) == 0)
        {
          if (g_strcmp(text, "127.0.0.1") != 0)
          {
            use_uds = 0;
          }
        }
        if (use_uds)
        {
          g_snprintf(text, 255, "/tmp/.xrdp/xrdp_display_%d", self->display);
        }
        else
        {
          g_snprintf(text, 255, "%d", 6200 + self->display);
        }
      }
      else
      {
        g_set_wait_obj(self->wm->pro_layer->self_term_event); /* kill session */
      }
    }
  }
  if (!g_is_wait_obj_set(self->wm->pro_layer->self_term_event))
  {
    /* this adds the port to the end of the list, it will already be in
       the list as -1
       the module should use the last one */
    if (g_strlen(text) > 0)
    {
      list_add_item(self->login_names, (long)g_strdup("port"));
      list_add_item(self->login_values, (long)g_strdup(text));
    }
    /* always set these */

    self->mod->mod_set_param(self->mod, "client_info",
                             (char*)(self->wm->session->client_info));

    name = self->wm->session->client_info->hostname;
    self->mod->mod_set_param(self->mod, "hostname", name);
    g_snprintf(text, 255, "%d", self->wm->session->client_info->keylayout);
    self->mod->mod_set_param(self->mod, "keylayout", text);
    for (i = 0; i < self->login_names->count; i++)
    {
      name = (char*)list_get_item(self->login_names, i);
      value = (char*)list_get_item(self->login_values, i);
      self->mod->mod_set_param(self->mod, name, value);
    }
    /* connect */
    if (self->mod->mod_connect(self->mod) == 0)
    {
      rv = 0; /* connect success */
    }
  }
  if (rv == 0)
  {
    /* sync modifiers */
    key_flags = 0;
    device_flags = 0;
    if (self->wm->scroll_lock)
    {
      key_flags |= 1;
    }
    if (self->wm->num_lock)
    {
      key_flags |= 2;
    }
    if (self->wm->caps_lock)
    {
      key_flags |= 4;
    }
    if (self->mod != 0)
    {
      if (self->mod->mod_event != 0)
      {
        self->mod->mod_event(self->mod, 17, key_flags, device_flags,
                             key_flags, device_flags);
      }
    }
  }
  return rv;
}

/*****************************************************************************/
/* returns error
   send a list of channels to the channel handler */
static int APP_CC
xrdp_mm_trans_send_channel_setup(struct xrdp_mm* self, struct trans* trans)
{
  int index;
  int chan_id;
  int chan_flags;
  int size;
  struct stream* s;
  char chan_name[256];

  g_memset(chan_name,0,sizeof(char) * 256);

  s = trans_get_out_s(trans, 8192);
  if (s == 0)
  {
    return 1;
  }
  s_push_layer(s, iso_hdr, 8);
  s_push_layer(s, mcs_hdr, 8);
  s_push_layer(s, sec_hdr, 2);
  index = 0;
  while (libxrdp_query_channel(self->wm->session, index, chan_name,
                               &chan_flags) == 0)
  {
    chan_id = libxrdp_get_channel_id(self->wm->session, chan_name);
    out_uint8a(s, chan_name, 8);
    out_uint16_le(s, chan_id);
    out_uint16_le(s, chan_flags);
    index++;
  }
  s_mark_end(s);
  s_pop_layer(s, sec_hdr);
  out_uint16_le(s, index);
  s_pop_layer(s, mcs_hdr);
  size = (int)(s->end - s->p);
  out_uint32_le(s, 3); /* msg id */
  out_uint32_le(s, size); /* msg size */
  s_pop_layer(s, iso_hdr);
  size = (int)(s->end - s->p);
  out_uint32_le(s, 0); /* version */
  out_uint32_le(s, size); /* block size */
  return trans_force_write(trans);
}

/*****************************************************************************/
/* returns error */
static int APP_CC
xrdp_mm_trans_send_channel_data_response(struct xrdp_mm* self,
                                         struct trans* trans)
{
  struct stream* s;

  s = trans_get_out_s(trans, 8192);
  if (s == 0)
  {
    return 1;
  }
  out_uint32_le(s, 0); /* version */
  out_uint32_le(s, 8 + 8); /* size */
  out_uint32_le(s, 7); /* msg id */
  out_uint32_le(s, 8); /* size */
  s_mark_end(s);
  return trans_force_write(trans);
}

/*****************************************************************************/
/* returns error
   init is done, sent channel setup */
static int APP_CC
xrdp_mm_trans_process_init_response(struct xrdp_mm* self, struct trans* trans)
{
  return xrdp_mm_trans_send_channel_setup(self, trans);
}

/*****************************************************************************/
/* returns error
   data coming in from the channel handler, send it to the client */
static int APP_CC
xrdp_mm_trans_process_channel_data(struct xrdp_mm* self, struct trans* trans)
{
  struct stream* s;
  int size;
  int total_size;
  int chan_id;
  int chan_flags;
  int rv;

  s = trans_get_in_s(trans);
  if (s == 0)
  {
    return 1;
  }
  in_uint16_le(s, chan_id);
  in_uint16_le(s, chan_flags);
  in_uint16_le(s, size);
  in_uint32_le(s, total_size);
  rv = xrdp_mm_trans_send_channel_data_response(self, trans);
  if (rv == 0)
  {
    rv = libxrdp_send_to_channel(self->wm->session, chan_id, s->p, size, total_size,
                                 chan_flags);
  }
  return rv;
}

/*****************************************************************************/
/* returns error
   process a message for the channel handler */
static int APP_CC
xrdp_mm_chan_process_msg(struct xrdp_mm* self, struct trans* trans,
                         struct stream* s)
{
  int rv;
  int id;
  int size;
  char* next_msg;

  rv = 0;
  while (s_check_rem(s, 8))
  {
    next_msg = s->p;
    in_uint32_le(s, id);
    in_uint32_le(s, size);
    next_msg += size;
    switch (id)
    {
      case 2: /* channel init response */
        rv = xrdp_mm_trans_process_init_response(self, trans);
        break;
      case 4: /* channel setup response */
        break;
      case 6: /* channel data response */
        break;
      case 8: /* channel data */
        rv = xrdp_mm_trans_process_channel_data(self, trans);
        break;
      default:
        g_writeln("xrdp_mm_chan_process_msg: unknown id %d", id);
        break;
    }
    if (rv != 0)
    {
      break;
    }
    s->p = next_msg;
  }
  return rv;
}

/*****************************************************************************/
/* this is callback from trans obj
   returns error */
static int APP_CC
xrdp_mm_chan_data_in(struct trans* trans)
{
  struct xrdp_mm* self;
  struct stream* s;
  int id;
  int size;
  int error;

  if (trans == 0)
  {
    return 1;
  }
  self = (struct xrdp_mm*)(trans->callback_data);
  s = trans_get_in_s(trans);
  if (s == 0)
  {
    return 1;
  }
  in_uint32_le(s, id);
  in_uint32_le(s, size);
  error = trans_force_read(trans, size - 8);
  if (error == 0)
  {
    /* here, the entire message block is read in, process it */
    error = xrdp_mm_chan_process_msg(self, trans, s);
  }
  return error;
}

/*****************************************************************************/
static int APP_CC
xrdp_mm_chan_send_init(struct xrdp_mm* self)
{
  struct stream* s;

  s = trans_get_out_s(self->chan_trans, 8192);
  if (s == 0)
  {
    return 1;
  }
  out_uint32_le(s, 0); /* version */
  out_uint32_le(s, 8 + 8); /* size */
  out_uint32_le(s, 1); /* msg id */
  out_uint32_le(s, 8); /* size */
  s_mark_end(s);
  return trans_force_write(self->chan_trans);
}

/*****************************************************************************/
/* connect to chansrv */
static int APP_CC
xrdp_mm_connect_chansrv(struct xrdp_mm* self, char* ip, char* port)
{
  int index;

  self->usechansrv = 1;

  /* connect channel redir */
  if ((ip == 0) || (strcmp(ip, "127.0.0.1") == 0) || (ip[0] == 0))
  {
    /* unix socket */
    self->chan_trans = trans_create(TRANS_MODE_UNIX, 8192, 8192);
  }
  else
  {
    /* tcp */
    self->chan_trans = trans_create(TRANS_MODE_TCP, 8192, 8192);
  }
  self->chan_trans->trans_data_in = xrdp_mm_chan_data_in;
  self->chan_trans->header_size = 8;
  self->chan_trans->callback_data = self;
  /* try to connect up to 4 times */
  for (index = 0; index < 4; index++)
  {
    if (trans_connect(self->chan_trans, ip, port, 3000) == 0)
    {
      self->chan_trans_up = 1;
      break;
    }
    g_sleep(1000);
    g_writeln("xrdp_mm_connect_chansrv: connect failed "
              "trying again...");
  }
  if (!(self->chan_trans_up))
  {
    g_writeln("xrdp_mm_connect_chansrv: error in trans_connect "
              "chan");
  }
  if (self->chan_trans_up)
  {
    if (xrdp_mm_chan_send_init(self) != 0)
    {
      g_writeln("xrdp_mm_connect_chansrv: error in "
                "xrdp_mm_chan_send_init");
    }
    else
    {
      g_writeln("xrdp_mm_connect_chansrv: chansrv connect successful");
    }
  }
  return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_mm_process_login_response(struct xrdp_mm* self, struct stream* s)
{
  int ok;
  int display;
  int rv;
  int uid;
  int gid;
  char text[256];
  char ip[256];
  char port[256];

  rv = 0;
  in_uint16_be(s, ok);
  in_uint16_be(s, display);
  if (ok)
  {
    self->display = display;
    g_snprintf(text, 255, "xrdp_mm_process_login_response: login successful "
                          "for display %d", display);
    xrdp_wm_log_msg(self->wm, text);
    if (xrdp_mm_setup_mod1(self) == 0)
    {
      if (xrdp_mm_setup_mod2(self) == 0)
      {
        xrdp_mm_get_value(self, "ip", ip, 255);
        xrdp_wm_set_login_mode(self->wm, 10);
        self->wm->dragging = 0;
        /* connect channel redir */
        if ((ip == 0) || (strcmp(ip, "127.0.0.1") == 0) || (ip[0] == 0))
        {
          g_snprintf(port, 255, "/tmp/.xrdp/xrdp_chansrv_socket_%d", 7200 + display);
        }
        else
        {
          g_snprintf(port, 255, "%d", 7200 + display);
        }
        xrdp_mm_connect_chansrv(self, ip, port);
      }
    }
  }
  else
  {
    xrdp_wm_log_msg(self->wm, "xrdp_mm_process_login_response: "
                              "login failed");
  }
  self->delete_sesman_trans = 1;
  self->connected_state = 0;
  if (self->wm->login_mode != 10)
  {
    xrdp_wm_set_login_mode(self->wm, 11);
    xrdp_mm_module_cleanup(self);
  }

  return rv;
}

/*****************************************************************************/
static int
xrdp_mm_get_sesman_port(char* port, int port_bytes)
{
  int fd;
  int error;
  int index;
  char* val;
  char cfg_file[256];
  struct list* names;
  struct list* values;

  g_memset(cfg_file,0,sizeof(char) * 256);
  /* default to port 3350 */
  g_strncpy(port, "3350", port_bytes - 1);
  /* see if port is in xrdp.ini file */
  g_snprintf(cfg_file, 255, "%s/sesman.ini", XRDP_CFG_PATH);
  fd = g_file_open(cfg_file);
  if (fd > 0)
  {
    names = list_create();
    names->auto_free = 1;
    values = list_create();
    values->auto_free = 1;
    if (file_read_section(fd, "Globals", names, values) == 0)
    {
      for (index = 0; index < names->count; index++)
      {
        val = (char*)list_get_item(names, index);
        if (val != 0)
        {
          if (g_strcasecmp(val, "ListenPort") == 0)
          {
            val = (char*)list_get_item(values, index);
            error = g_atoi(val);
            if ((error > 0) && (error < 65000))
            {
              g_strncpy(port, val, port_bytes - 1);
            }
            break;
          }
        }
      }
    }
    list_delete(names);
    list_delete(values);
    g_file_close(fd);
  }

  return 0;
}

/*****************************************************************************/
/* returns error
   data coming from client that need to go to channel handler */
int APP_CC
xrdp_mm_process_channel_data(struct xrdp_mm* self, tbus param1, tbus param2,
                             tbus param3, tbus param4)
{
  struct stream* s;
  int rv;
  int length;
  int total_length;
  int flags;
  int id;
  char* data;

  rv = 0;
  if ((self->chan_trans != 0) && self->chan_trans_up)
  {
    s = trans_get_out_s(self->chan_trans, 8192);
    if (s != 0)
    {
      id = LOWORD(param1);
      flags = HIWORD(param1);
      length = param2;
      data = (char*)param3;
      total_length = param4;
      if (total_length < length)
      {
        g_writeln("WARNING in xrdp_mm_process_channel_data(): total_len < length");
        total_length = length;
      }
      out_uint32_le(s, 0); /* version */
      out_uint32_le(s, 8 + 8 + 2 + 2 + 2 + 4 + length);
      out_uint32_le(s, 5); /* msg id */
      out_uint32_le(s, 8 + 2 + 2 + 2 + 4 + length);
      out_uint16_le(s, id);
      out_uint16_le(s, flags);
      out_uint16_le(s, length);
      out_uint32_le(s, total_length);
      out_uint8a(s, data, length);
      s_mark_end(s);
      rv = trans_force_write(self->chan_trans);
    }
  }

  return rv;
}

/*****************************************************************************/
static int APP_CC
xrdp_mm_sesman_data_in(struct trans* trans)
{
  struct xrdp_mm* self;
  struct stream* s;
  int version;
  int size;
  int error;
  int code;

  if (trans == 0)
  {
    return 1;
  }
  self = (struct xrdp_mm*)(trans->callback_data);
  s = trans_get_in_s(trans);
  if (s == 0)
  {
    return 1;
  }
  in_uint32_be(s, version);
  in_uint32_be(s, size);
  error = trans_force_read(trans, size - 8);
  if (error == 0)
  {
    in_uint16_be(s, code);
    switch (code)
    {
      case 3:
        error = xrdp_mm_process_login_response(self, s);
        break;
      default:
        g_writeln("xrdp_mm_sesman_data_in: unknown code %d", code);
        break;
    }
  }

  return error;
}

/*****************************************************************************/
int APP_CC
xrdp_mm_connect(struct xrdp_mm* self)
{
  struct list* names;
  struct list* values;
  int index;
  int count;
  int use_sesman;
  int ok;
  int rv;
  char* name;
  char* value;
  char ip[256];
  char errstr[256];
  char text[256];
  char port[8];
  char chansrvport[256];

  g_memset(ip, 0, sizeof(ip));
  g_memset(errstr, 0, sizeof(errstr));
  g_memset(text, 0, sizeof(text));
  g_memset(port, 0, sizeof(port));
  g_memset(chansrvport, 0, sizeof(chansrvport));
  rv = 0; /* success */
  use_sesman = 0;
  names = self->login_names;
  values = self->login_values;
  count = names->count;
  for (index = 0; index < count; index++)
  {
    name = (char*)list_get_item(names, index);
    value = (char*)list_get_item(values, index);
    if (g_strcasecmp(name, "ip") == 0)
    {
      g_strncpy(ip, value, 255);
    }
    else if (g_strcasecmp(name, "port") == 0)
    {
      if (g_strcasecmp(value, "-1") == 0)
      {
        use_sesman = 1;
      }
    }
    else if (g_strcasecmp(name, "chansrvport") == 0)
    {
      g_strncpy(chansrvport, value, 255);
      self->usechansrv = 1;
    }
  }
  if (use_sesman)
  {
    ok = 0;
    errstr[0] = 0;
    trans_delete(self->sesman_trans);
    self->sesman_trans = trans_create(TRANS_MODE_TCP, 8192, 8192);
    xrdp_mm_get_sesman_port(port, sizeof(port));
    g_snprintf(text, 255, "connecting to sesman ip %s port %s", ip, port);
    xrdp_wm_log_msg(self->wm, text);

    self->sesman_trans->trans_data_in = xrdp_mm_sesman_data_in;
    self->sesman_trans->header_size = 8;
    self->sesman_trans->callback_data = self;
    /* try to connect up to 4 times */
    for (index = 0; index < 4; index++)
    {
      if (trans_connect(self->sesman_trans, ip, port, 3000) == 0)
      {
        self->sesman_trans_up = 1;
        ok = 1;
        break;
      }
      g_sleep(1000);
      g_writeln("xrdp_mm_connect: connect failed "
                "trying again...");
    }
    if (ok)
    {
      /* fully connect */
      xrdp_wm_log_msg(self->wm, "sesman connect ok");
      self->connected_state = 1;
      rv = xrdp_mm_send_login(self);
    }
    else
    {
      xrdp_wm_log_msg(self->wm, errstr);
      trans_delete(self->sesman_trans);
      self->sesman_trans = 0;
      self->sesman_trans_up = 0;
      rv = 1;
    }
  }
  else /* no sesman */
  {
    if (xrdp_mm_setup_mod1(self) == 0)
    {
      if (xrdp_mm_setup_mod2(self) == 0)
      {
        xrdp_wm_set_login_mode(self->wm, 10);
      }
      else
      {
        /* connect error */
        g_snprintf(errstr, 255, "Failure to connect to: %s port: %s",
                   ip, port);
        xrdp_wm_log_msg(self->wm, errstr);
        rv = 1 ; /* failure */
      }
    }
    if (self->wm->login_mode != 10)
    {
      xrdp_wm_set_login_mode(self->wm, 11);
      xrdp_mm_module_cleanup(self);
    }
  }
  self->sesman_controlled = use_sesman;

  if ((self->wm->login_mode == 10) && (self->sesman_controlled == 0) &&
      (self->usechansrv != 0))
  {
    /* if sesman controlled, this will connect later */
    xrdp_mm_connect_chansrv(self, "", chansrvport);
  }

  return rv;
}

/*****************************************************************************/
int APP_CC
xrdp_mm_get_wait_objs(struct xrdp_mm* self,
                      tbus* read_objs, int* rcount,
                      tbus* write_objs, int* wcount, int* timeout)
{
  int rv = 0;

  if (self == 0)
  {
    return 0;
  }
  rv = 0;
  if ((self->sesman_trans != 0) && self->sesman_trans_up)
  {
    trans_get_wait_objs(self->sesman_trans, read_objs, rcount);
  }
  if ((self->chan_trans != 0) && self->chan_trans_up)
  {
    trans_get_wait_objs(self->chan_trans, read_objs, rcount);
  }
  if (self->mod != 0)
  {
    if (self->mod->mod_get_wait_objs != 0)
    {
      rv = self->mod->mod_get_wait_objs(self->mod, read_objs, rcount,
                                        write_objs, wcount, timeout);
    }
  }

  return rv;
}

/*****************************************************************************/
int APP_CC
xrdp_mm_check_wait_objs(struct xrdp_mm* self)
{
  int rv;

  if (self == 0)
  {
    return 0;
  }
  rv = 0;
  if ((self->sesman_trans != 0) && self->sesman_trans_up)
  {
    if (trans_check_wait_objs(self->sesman_trans) != 0)
    {
      self->delete_sesman_trans = 1;
    }
  }
  if ((self->chan_trans != 0) && self->chan_trans_up)
  {
    if (trans_check_wait_objs(self->chan_trans) != 0)
    {
      self->delete_chan_trans = 1;
    }
  }
  if (self->mod != 0)
  {
    if (self->mod->mod_check_wait_objs != 0)
    {
      rv = self->mod->mod_check_wait_objs(self->mod);
    }
  }
  if (self->delete_sesman_trans)
  {
    trans_delete(self->sesman_trans);
    self->sesman_trans = 0;
    self->sesman_trans_up = 0;
    self->delete_sesman_trans = 0;
  }
  if (self->delete_chan_trans)
  {
    trans_delete(self->chan_trans);
    self->chan_trans = 0;
    self->chan_trans_up = 0;
    self->delete_chan_trans = 0;
  }

  return rv;
}

#if 0
/*****************************************************************************/
struct xrdp_painter* APP_CC
get_painter(struct xrdp_mod* mod)
{
  struct xrdp_wm* wm;
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)(mod->painter);
  if (p == 0)
  {
    wm = (struct xrdp_wm*)(mod->wm);
    p = xrdp_painter_create(wm, wm->session);
    mod->painter = (tintptr)p;
  }
  return p;
}
#endif

/*****************************************************************************/
int DEFAULT_CC
server_begin_update(struct xrdp_mod* mod)
{
  struct xrdp_wm* wm;
  struct xrdp_painter* p;

  wm = (struct xrdp_wm*)(mod->wm);
  p = xrdp_painter_create(wm, wm->session);
  xrdp_painter_begin_update(p);
  mod->painter = (long)p;
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_end_update(struct xrdp_mod* mod)
{
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)(mod->painter);
  if (p == 0)
  {
    return 0;
  }
  xrdp_painter_end_update(p);
  xrdp_painter_delete(p);
  mod->painter = 0;
  return 0;
}

/*****************************************************************************/
/* got bell signal... try to send to client */
int DEFAULT_CC
server_bell_trigger(struct xrdp_mod* mod)
{
  struct xrdp_wm* wm;

  wm = (struct xrdp_wm*)(mod->wm);
  xrdp_wm_send_bell(wm);
  return 0;
}


/*****************************************************************************/
int DEFAULT_CC
server_fill_rect(struct xrdp_mod* mod, int x, int y, int cx, int cy)
{
  struct xrdp_wm* wm;
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)(mod->painter);
  if (p == 0)
  {
    return 0;
  }
  wm = (struct xrdp_wm*)(mod->wm);
  xrdp_painter_fill_rect(p, wm->target_surface, x, y, cx, cy);
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_screen_blt(struct xrdp_mod* mod, int x, int y, int cx, int cy,
                  int srcx, int srcy)
{
  struct xrdp_wm* wm;
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)(mod->painter);
  if (p == 0)
  {
    return 0;
  }
  wm = (struct xrdp_wm*)(mod->wm);
  p->rop = 0xcc;
  xrdp_painter_copy(p, wm->screen, wm->target_surface, x, y, cx, cy, srcx, srcy);
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_paint_rect(struct xrdp_mod* mod, int x, int y, int cx, int cy,
                  char* data, int width, int height, int srcx, int srcy)
{
  struct xrdp_wm* wm;
  struct xrdp_bitmap* b;
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)(mod->painter);
  if (p == 0)
  {
    return 0;
  }
  wm = (struct xrdp_wm*)(mod->wm);
  b = xrdp_bitmap_create_with_data(width, height, wm->screen->bpp, data, wm);
  xrdp_painter_copy(p, b, wm->target_surface, x, y, cx, cy, srcx, srcy);
  xrdp_bitmap_delete(b);
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_set_pointer(struct xrdp_mod* mod, int x, int y,
                   char* data, char* mask)
{
  struct xrdp_wm* wm;

  wm = (struct xrdp_wm*)(mod->wm);
  xrdp_wm_pointer(wm, data, mask, x, y);
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_palette(struct xrdp_mod* mod, int* palette)
{
  struct xrdp_wm* wm;

  wm = (struct xrdp_wm*)(mod->wm);
  if (g_memcmp(wm->palette, palette, 255 * sizeof(int)) != 0)
  {
    g_memcpy(wm->palette, palette, 256 * sizeof(int));
    xrdp_wm_send_palette(wm);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_msg(struct xrdp_mod* mod, char* msg, int code)
{
  struct xrdp_wm* wm;

  if (code == 1)
  {
    g_writeln(msg);
    return 0;
  }
  wm = (struct xrdp_wm*)(mod->wm);
  return xrdp_wm_log_msg(wm, msg);
}

/*****************************************************************************/
int DEFAULT_CC
server_is_term(struct xrdp_mod* mod)
{
  return g_is_term();
}

/*****************************************************************************/
int DEFAULT_CC
server_set_clip(struct xrdp_mod* mod, int x, int y, int cx, int cy)
{
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)(mod->painter);
  if (p == 0)
  {
    return 0;
  }
  return xrdp_painter_set_clip(p, x, y, cx, cy);
}

/*****************************************************************************/
int DEFAULT_CC
server_reset_clip(struct xrdp_mod* mod)
{
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)(mod->painter);
  if (p == 0)
  {
    return 0;
  }
  return xrdp_painter_clr_clip(p);
}

/*****************************************************************************/
int DEFAULT_CC
server_set_fgcolor(struct xrdp_mod* mod, int fgcolor)
{
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)(mod->painter);
  if (p == 0)
  {
    return 0;
  }
  p->fg_color = fgcolor;
  p->pen.color = p->fg_color;
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_set_bgcolor(struct xrdp_mod* mod, int bgcolor)
{
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)(mod->painter);
  if (p == 0)
  {
    return 0;
  }
  p->bg_color = bgcolor;
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_set_opcode(struct xrdp_mod* mod, int opcode)
{
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)(mod->painter);
  if (p == 0)
  {
    return 0;
  }
  p->rop = opcode;
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_set_mixmode(struct xrdp_mod* mod, int mixmode)
{
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)(mod->painter);
  if (p == 0)
  {
    return 0;
  }
  p->mix_mode = mixmode;
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_set_brush(struct xrdp_mod* mod, int x_orgin, int y_orgin,
                 int style, char* pattern)
{
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)(mod->painter);
  if (p == 0)
  {
    return 0;
  }
  p->brush.x_orgin = x_orgin;
  p->brush.y_orgin = y_orgin;
  p->brush.style = style;
  g_memcpy(p->brush.pattern, pattern, 8);
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_set_pen(struct xrdp_mod* mod, int style, int width)
{
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)(mod->painter);
  if (p == 0)
  {
    return 0;
  }
  p->pen.style = style;
  p->pen.width = width;
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_draw_line(struct xrdp_mod* mod, int x1, int y1, int x2, int y2)
{
  struct xrdp_wm* wm;
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)(mod->painter);
  if (p == 0)
  {
    return 0;
  }
  wm = (struct xrdp_wm*)(mod->wm);
  return xrdp_painter_line(p, wm->target_surface, x1, y1, x2, y2);
}

/*****************************************************************************/
int DEFAULT_CC
server_add_char(struct xrdp_mod* mod, int font, int charactor,
                int offset, int baseline,
                int width, int height, char* data)
{
  struct xrdp_font_char fi;

  fi.offset = offset;
  fi.baseline = baseline;
  fi.width = width;
  fi.height = height;
  fi.incby = 0;
  fi.data = data;
  return libxrdp_orders_send_font(((struct xrdp_wm*)mod->wm)->session,
                                  &fi, font, charactor);
}

/*****************************************************************************/
int DEFAULT_CC
server_draw_text(struct xrdp_mod* mod, int font,
                 int flags, int mixmode, int clip_left, int clip_top,
                 int clip_right, int clip_bottom,
                 int box_left, int box_top,
                 int box_right, int box_bottom,
                 int x, int y, char* data, int data_len)
{
  struct xrdp_wm* wm;
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)(mod->painter);
  if (p == 0)
  {
    return 0;
  }
  wm = (struct xrdp_wm*)(mod->wm);
  return xrdp_painter_draw_text2(p, wm->target_surface, font, flags,
                                 mixmode, clip_left, clip_top,
                                 clip_right, clip_bottom,
                                 box_left, box_top,
                                 box_right, box_bottom,
                                 x, y, data, data_len);
}

/*****************************************************************************/
int DEFAULT_CC
server_reset(struct xrdp_mod* mod, int width, int height, int bpp)
{
  struct xrdp_wm* wm;

  wm = (struct xrdp_wm*)(mod->wm);
  if (wm->client_info == 0)
  {
    return 1;
  }
  /* older client can't resize */
  if (wm->client_info->build <= 419)
  {
    return 0;
  }
  /* if same, don't need to do anything */
  if (wm->client_info->width == width &&
      wm->client_info->height == height &&
      wm->client_info->bpp == bpp)
  {
    return 0;
  }
  /* reset lib, client_info gets updated in libxrdp_reset */
  if (libxrdp_reset(wm->session, width, height, bpp) != 0)
  {
    return 1;
  }
  /* reset cache */
  xrdp_cache_reset(wm->cache, wm->client_info);
  /* resize the main window */
  xrdp_bitmap_resize(wm->screen, wm->client_info->width,
                     wm->client_info->height);
  /* load some stuff */
  xrdp_wm_load_static_colors_plus(wm, 0);
  xrdp_wm_load_static_pointers(wm);
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_query_channel(struct xrdp_mod* mod, int index, char* channel_name,
                     int* channel_flags)
{
  struct xrdp_wm* wm;

  wm = (struct xrdp_wm*)(mod->wm);
  if (wm->mm->usechansrv)
  {
    return 1;
  }
  return libxrdp_query_channel(wm->session, index, channel_name,
                               channel_flags);
}

/*****************************************************************************/
/* returns -1 on error */
int DEFAULT_CC
server_get_channel_id(struct xrdp_mod* mod, char* name)
{
  struct xrdp_wm* wm;

  wm = (struct xrdp_wm*)(mod->wm);
  if (wm->mm->usechansrv)
  {
    return -1;
  }
  return libxrdp_get_channel_id(wm->session, name);
}

/*****************************************************************************/
int DEFAULT_CC
server_send_to_channel(struct xrdp_mod* mod, int channel_id,
                       char* data, int data_len,
                       int total_data_len, int flags)
{
  struct xrdp_wm* wm;

  wm = (struct xrdp_wm*)(mod->wm);
  if (wm->mm->usechansrv)
  {
    return 1;
  }
  return libxrdp_send_to_channel(wm->session, channel_id, data, data_len,
                                 total_data_len, flags);
}

/*****************************************************************************/
int DEFAULT_CC
server_create_os_surface(struct xrdp_mod* mod, int rdpindex,
                         int width, int height)
{
  struct xrdp_wm* wm;
  struct xrdp_bitmap* bitmap;
  int error;

  wm = (struct xrdp_wm*)(mod->wm);
  bitmap = xrdp_bitmap_create(width, height, wm->screen->bpp,
                              WND_TYPE_OFFSCREEN, wm);
  error = xrdp_cache_add_os_bitmap(wm->cache, bitmap, rdpindex);
  if (error != 0)
  {
    g_writeln("server_create_os_surface: xrdp_cache_add_os_bitmap failed");
    return 1;
  }
  bitmap->item_index = rdpindex;
  bitmap->id = rdpindex;
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_switch_os_surface(struct xrdp_mod* mod, int rdpindex)
{
  struct xrdp_wm* wm;
  struct xrdp_os_bitmap_item* bi;
  struct xrdp_painter* p;

  //g_writeln("server_switch_os_surface: id 0x%x", id);
  wm = (struct xrdp_wm*)(mod->wm);
  if (rdpindex == -1)
  {
    //g_writeln("server_switch_os_surface: setting target_surface to screen");
    wm->target_surface = wm->screen;
    p = (struct xrdp_painter*)(mod->painter);
    if (p != 0)
    {
      //g_writeln("setting target");
      wm_painter_set_target(p);
    }
    return 0;
  }
  bi = xrdp_cache_get_os_bitmap(wm->cache, rdpindex);
  if (bi != 0)
  {
    //g_writeln("server_switch_os_surface: setting target_surface to rdpid %d", id);
    wm->target_surface = bi->bitmap;
    p = (struct xrdp_painter*)(mod->painter);
    if (p != 0)
    {
      //g_writeln("setting target");
      wm_painter_set_target(p);
    }
  }
  else
  {
    g_writeln("server_switch_os_surface: error finding id %d", rdpindex);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_delete_os_surface(struct xrdp_mod* mod, int rdpindex)
{
  struct xrdp_wm* wm;
  struct xrdp_painter* p;

  //g_writeln("server_delete_os_surface: id 0x%x", id);
  wm = (struct xrdp_wm*)(mod->wm);
  if (wm->target_surface->type == WND_TYPE_OFFSCREEN)
  {
    if (wm->target_surface->id == rdpindex)
    {
      g_writeln("server_delete_os_surface: setting target_surface to screen");
      wm->target_surface = wm->screen;
      p = (struct xrdp_painter*)(mod->painter);
      if (p != 0)
      {
        //g_writeln("setting target");
        wm_painter_set_target(p);
      }
    }
  }
  xrdp_cache_remove_os_bitmap(wm->cache, rdpindex);
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_paint_rect_os(struct xrdp_mod* mod, int x, int y, int cx, int cy,
                     int rdpindex, int srcx, int srcy)
{
  struct xrdp_wm* wm;
  struct xrdp_bitmap* b;
  struct xrdp_painter* p;
  struct xrdp_os_bitmap_item* bi;

  p = (struct xrdp_painter*)(mod->painter);
  if (p == 0)
  {
    return 0;
  }
  wm = (struct xrdp_wm*)(mod->wm);
  bi = xrdp_cache_get_os_bitmap(wm->cache, rdpindex);
  if (bi != 0)
  {
    b = bi->bitmap;
    xrdp_painter_copy(p, b, wm->target_surface, x, y, cx, cy, srcx, srcy);
  }
  else
  {
    g_writeln("server_paint_rect_os: error finding id %d", rdpindex);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_set_hints(struct xrdp_mod* mod, int hints, int mask)
{
  struct xrdp_wm* wm;

  wm = (struct xrdp_wm*)(mod->wm);
  if (mask & 1)
  {
    if (hints & 1)
    {
      wm->hints |= 1;
    }
    else
    {
      wm->hints &= ~1;
    }
  }
  return 0;
}
