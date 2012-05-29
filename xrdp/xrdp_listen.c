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

   listen for incoming connection

*/

#include "xrdp.h"

/* 'g_process' is protected by the semaphore 'g_process_sem'.  One thread sets
   g_process and waits for the other to process it */
static tbus g_process_sem = 0;
static struct xrdp_process* g_process = 0;

/*****************************************************************************/
static int
xrdp_listen_create_pro_done(struct xrdp_listen* self)
{
  int pid;
  char text[256];

  pid = g_getpid();
  g_snprintf(text, 255, "xrdp_%8.8x_listen_pro_done_event", pid);
  self->pro_done_event = g_create_wait_obj(text);
  if(self->pro_done_event == 0)
  {
    g_writeln("Failure creating pro_done_event");
  }
  return 0;
}

/*****************************************************************************/
struct xrdp_listen* APP_CC
xrdp_listen_create(void)
{
  struct xrdp_listen* self;

  self = (struct xrdp_listen*)g_malloc(sizeof(struct xrdp_listen), 1);
  xrdp_listen_create_pro_done(self);
  self->process_list = list_create();
  if (g_process_sem == 0)
  {
    g_process_sem = tc_sem_create(0);
  }
  self->listen_trans = trans_create(TRANS_MODE_TCP, 16, 16);
  if (self->listen_trans == 0)
  {
    g_writeln("xrdp_listen_create: trans_create failed");
  }
  return self;
}

/*****************************************************************************/
void APP_CC
xrdp_listen_delete(struct xrdp_listen* self)
{
  if (self->listen_trans != 0)
  {
    trans_delete(self->listen_trans);
  }
  if (g_process_sem != 0)
  {
    tc_sem_delete(g_process_sem);
    g_process_sem = 0;
  }
  g_delete_wait_obj(self->pro_done_event);
  list_delete(self->process_list);
  g_free(self);
}

/*****************************************************************************/
/* returns error */
static int APP_CC
xrdp_listen_add_pro(struct xrdp_listen* self, struct xrdp_process* process)
{
  list_add_item(self->process_list, (tbus)process);
  return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_listen_delete_done_pro(struct xrdp_listen* self)
{
  int i;
  struct xrdp_process* pro;

  for (i = self->process_list->count - 1; i >= 0; i--)
  {
    pro = (struct xrdp_process*)list_get_item(self->process_list, i);
    if (pro != 0)
    {
      if (pro->status < 0)
      {
        xrdp_process_delete(pro);
        list_remove_item(self->process_list, i);
      }
    }
  }
  return 0;
}

/*****************************************************************************/
/* i can't get stupid in_val to work, hum using global var for now */
THREAD_RV THREAD_CC
xrdp_process_run(void* in_val)
{
  struct xrdp_process* process;

  DEBUG(("process started"));
  process = g_process;
  g_process = 0;
  tc_sem_inc(g_process_sem);
  xrdp_process_main_loop(process);
  DEBUG(("process done"));
  return 0;
}

/*****************************************************************************/
static int
xrdp_listen_get_port_address(char* port, int port_bytes,
                             char* address, int address_bytes,
                             struct xrdp_startup_params* startup_param)
{
  int fd;
  int error;
  int index;
  char* val;
  struct list* names;
  struct list* values;
  char cfg_file[256];

  /* default to port 3389 */
  g_strncpy(port, "3389", port_bytes - 1);
  /* Default to all */
  g_strncpy(address, "0.0.0.0", address_bytes - 1);
  /* see if port or address is in xrdp.ini file */
  g_snprintf(cfg_file, 255, "%s/xrdp.ini", XRDP_CFG_PATH);
  fd = g_file_open(cfg_file);
  if (fd > 0)
  {
    names = list_create();
    names->auto_free = 1;
    values = list_create();
    values->auto_free = 1;
    if (file_read_section(fd, "globals", names, values) == 0)
    {
      for (index = 0; index < names->count; index++)
      {
        val = (char*)list_get_item(names, index);
        if (val != 0)
        {
          if (g_strcasecmp(val, "port") == 0)
          {
            val = (char*)list_get_item(values, index);
            error = g_atoi(val);
            if ((error > 0) && (error < 65000))
            {
              g_strncpy(port, val, port_bytes - 1);
            }
          }
          if (g_strcasecmp(val, "address") == 0)
          {
            val = (char*)list_get_item(values, index);
            g_strncpy(address, val, address_bytes - 1);
          }
          if (g_strcasecmp(val, "fork") == 0)
          {
            val = (char*)list_get_item(values, index);
            if ((g_strcasecmp(val, "yes") == 0) ||
                (g_strcasecmp(val, "on") == 0) ||
                (g_strcasecmp(val, "true") == 0) ||
                (g_atoi(val) != 0))
            {
              startup_param->fork = 1;
            }
          }
        }
      }
    }
    list_delete(names);
    list_delete(values);
    g_file_close(fd);
  }
  /* startup_param overrides */
  if (startup_param->port[0] != 0)
  {
    g_strncpy(port, startup_param->port, port_bytes - 1);
  }
  return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_listen_fork(struct xrdp_listen* self, struct trans* server_trans)
{
  int pid;
  struct xrdp_process* process;

  pid = g_fork();
  if (pid == 0)
  {
    /* child */
    /* recreate some main globals */
    xrdp_child_fork();
    /* recreate the process done wait object, not used in fork mode */
    /* close, don't delete this */
    g_close_wait_obj(self->pro_done_event);
    xrdp_listen_create_pro_done(self);
    /* delete listener, child need not listen */
    trans_delete(self->listen_trans);
    self->listen_trans = 0;
    /* new connect instance */
    process = xrdp_process_create(self, 0);
    process->server_trans = server_trans;
    g_process = process;
    xrdp_process_run(0);
    xrdp_process_delete(process);
    /* mark this process to exit */
    g_set_term(1);
    return 0;
  }
  /* parent */
  trans_delete(server_trans);
  return 0;
}

/*****************************************************************************/
/* a new connection is coming in */
int DEFAULT_CC
xrdp_listen_conn_in(struct trans* self, struct trans* new_self)
{
  struct xrdp_process* process;
  struct xrdp_listen* lis;

  lis = (struct xrdp_listen*)(self->callback_data);
  if (lis->startup_params->fork)
  {
    return xrdp_listen_fork(lis, new_self);
  }
  process = xrdp_process_create(lis, lis->pro_done_event);
  if (xrdp_listen_add_pro(lis, process) == 0)
  {
    /* start thread */
    process->server_trans = new_self;
    g_process = process;
    tc_thread_create(xrdp_process_run, 0);
    tc_sem_dec(g_process_sem); /* this will wait */
  }
  else
  {
    xrdp_process_delete(process);
  }
  return 0;
}

/*****************************************************************************/
/* wait for incoming connections */
int APP_CC
xrdp_listen_main_loop(struct xrdp_listen* self)
{
  int error;
  int robjs_count;
  int cont;
  int timeout = 0;
  char port[128];
  char address[256];
  tbus robjs[8];
  tbus term_obj;
  tbus sync_obj;
  tbus sck_obj;
  tbus done_obj;

  self->status = 1;
  if (xrdp_listen_get_port_address(port, sizeof(port),
                                   address, sizeof(address),
                                   self->startup_params) != 0)
  {
    g_writeln("xrdp_listen_main_loop: xrdp_listen_get_port failed");
    self->status = -1;
    return 1;
  }
  error = trans_listen_address(self->listen_trans, port, address);
  if (error == 0)
  {
    self->listen_trans->trans_conn_in = xrdp_listen_conn_in;
    self->listen_trans->callback_data = self;
    term_obj = g_get_term_event(); /*Global termination event */
    sync_obj = g_get_sync_event();
    done_obj = self->pro_done_event;
    cont = 1;
    while (cont)
    {
      /* build the wait obj list */
      robjs_count = 0;
      robjs[robjs_count++] = term_obj;
      robjs[robjs_count++] = sync_obj;
      robjs[robjs_count++] = done_obj;
      timeout = -1;
      if (trans_get_wait_objs(self->listen_trans, robjs, &robjs_count) != 0)
      {
	g_writeln("Listening socket is in wrong state we terminate listener") ;
        break;
      }
      /* wait - timeout -1 means wait indefinitely*/
      if (g_obj_wait(robjs, robjs_count, 0, 0, timeout) != 0)
      {
        /* error, should not get here */
        g_sleep(100);
      }
      if (g_is_wait_obj_set(term_obj)) /* termination called */
      {
        break;
      }
      if (g_is_wait_obj_set(sync_obj)) /* some function must be processed by this thread */
      {
        g_reset_wait_obj(sync_obj);
        g_process_waiting_function(); /* run the function */
      }
      if (g_is_wait_obj_set(done_obj)) /* pro_done_event */
      {
        g_reset_wait_obj(done_obj); 
	/* a process has died remove it from lists*/
        xrdp_listen_delete_done_pro(self);
      }
      /* Run the callback when accept() returns a new socket*/
      if (trans_check_wait_objs(self->listen_trans) != 0)
      {
        break;
      }
    }
    /* stop listening */
    trans_delete(self->listen_trans);
    self->listen_trans = 0;
    /* second loop to wait for all process threads to close */
    cont = 1;
    while (cont)
    {
      if (self->process_list->count == 0)
      {
        break;
      }
      timeout = -1;
      /* build the wait obj list */
      robjs_count = 0;
      robjs[robjs_count++] = sync_obj;
      robjs[robjs_count++] = done_obj;
      /* wait - timeout -1 means wait indefinitely*/
      if (g_obj_wait(robjs, robjs_count, 0, 0, timeout) != 0)
      {
        /* error, should not get here */
        g_sleep(100);
      }
      if (g_is_wait_obj_set(sync_obj)) /* some function must be processed by this thread */
      {
        g_reset_wait_obj(sync_obj);
        g_process_waiting_function(); /* run the function that is waiting*/
      }
      if (g_is_wait_obj_set(done_obj)) /* pro_done_event */
      {
        g_reset_wait_obj(done_obj);
        xrdp_listen_delete_done_pro(self);
      }
    }
  }
  else
  {
    g_writeln("xrdp_listen_main_loop: listen error, possible port "
              "already in use");
  }
  self->status = -1;
  return 0;
}
