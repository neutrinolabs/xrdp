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
   Copyright (C) Jay Sorg 2005-2008
*/

/**
 *
 * @file sesman.c
 * @brief Main program file
 * @author Jay Sorg
 *
 */

#include "sesman.h"

int g_sck;
int g_pid;
unsigned char g_fixedkey[8] = { 23, 82, 107, 6, 35, 78, 88, 7 };
struct config_sesman* g_cfg; /* defined in config.h */

tbus g_term_event = 0;
tbus g_sync_event = 0;

extern int g_thread_sck; /* in thread.c */

/******************************************************************************/
/**
 *
 * @brief Starts sesman main loop
 *
 */
static void DEFAULT_CC
sesman_main_loop(void)
{
  int in_sck;
  int error;
  int robjs_count;
  int cont;
  tbus sck_obj;
  tbus robjs[8];

  /*main program loop*/
  log_message(LOG_LEVEL_INFO, "listening...");
  g_sck = g_tcp_socket();
  g_tcp_set_non_blocking(g_sck);
  error = scp_tcp_bind(g_sck, g_cfg->listen_address, g_cfg->listen_port);
  if (error == 0)
  {
    error = g_tcp_listen(g_sck);
    if (error == 0)
    {
      sck_obj = g_create_wait_obj_from_socket(g_sck, 0);
      cont = 1;
      while (cont)
      {
        /* build the wait obj list */
        robjs_count = 0;
        robjs[robjs_count++] = sck_obj;
        robjs[robjs_count++] = g_term_event;
        robjs[robjs_count++] = g_sync_event;
        /* wait */
        if (g_obj_wait(robjs, robjs_count, 0, 0, -1) != 0)
        {
          /* error, should not get here */
          g_sleep(100);
        }
        if (g_is_wait_obj_set(g_term_event)) /* term */
        {
          break;
        }
        if (g_is_wait_obj_set(g_sync_event)) /* sync */
        {
          g_reset_wait_obj(g_sync_event);
          session_sync_start();
        }
        if (g_is_wait_obj_set(sck_obj)) /* incoming connection */
        {
          in_sck = g_tcp_accept(g_sck);
          if ((in_sck == -1) && g_tcp_last_error_would_block(g_sck))
          {
            /* should not get here */
            g_sleep(100);
          }
          else if (in_sck == -1)
          {
            /* error, should not get here */
            break;
          }
          else
          {
            /* we've got a connection, so we pass it to scp code */
            LOG_DBG("new connection");
            thread_scp_start(in_sck);
            /* todo, do we have to wait here ? */
          }
        }
      }
      g_delete_wait_obj_from_socket(sck_obj);
    }
    else
    {
      log_message(LOG_LEVEL_ERROR, "listen error %d (%s)",
                  g_get_errno(), g_get_strerror());
    }
  }
  else
  {
    log_message(LOG_LEVEL_ERROR, "bind error on "
                "port '%s': %d (%s)", g_cfg->listen_port,
                g_get_errno(), g_get_strerror());
  }
  g_tcp_close(g_sck);
}

/******************************************************************************/
int DEFAULT_CC
main(int argc, char** argv)
{
  int fd;
  enum logReturns error;
  int daemon = 1;
  int pid;
  char pid_s[8];
  char text[256];
  char pid_file[256];
  char cfg_file[256];

  g_init("xrdp-sesman");
  g_snprintf(pid_file, 255, "%s/xrdp-sesman.pid", XRDP_PID_PATH);
  if (1 == argc)
  {
    /* no options on command line. normal startup */
    g_printf("starting sesman...\n");
    daemon = 1;
  }
  else if ((2 == argc) && ((0 == g_strcasecmp(argv[1], "--nodaemon")) ||
                           (0 == g_strcasecmp(argv[1], "-nodaemon")) ||
                           (0 == g_strcasecmp(argv[1], "-n")) ||
                           (0 == g_strcasecmp(argv[1], "-ns"))))
  {
    /* starts sesman not daemonized */
    g_printf("starting sesman in foregroud...\n");
    daemon = 0;
  }
  else if ((2 == argc) && ((0 == g_strcasecmp(argv[1], "--help")) ||
                           (0 == g_strcasecmp(argv[1], "-help")) ||
                           (0 == g_strcasecmp(argv[1], "-h"))))
  {
    /* help screen */
    g_printf("sesman - xrdp session manager\n\n");
    g_printf("usage: sesman [command]\n\n");
    g_printf("command can be one of the following:\n");
    g_printf("-n, -ns, --nodaemon  starts sesman in foreground\n");
    g_printf("-k, --kill           kills running sesman\n");
    g_printf("-h, --help           shows this help\n");
    g_printf("if no command is specified, sesman is started in background");
    g_deinit();
    g_exit(0);
  }
  else if ((2 == argc) && ((0 == g_strcasecmp(argv[1], "--kill")) ||
                           (0 == g_strcasecmp(argv[1], "-kill")) ||
                           (0 == g_strcasecmp(argv[1], "-k"))))
  {
    /* killing running sesman */
    /* check if sesman is running */
    if (!g_file_exist(pid_file))
    {
      g_printf("sesman is not running (pid file not found - %s)\n", pid_file);
      g_deinit();
      g_exit(1);
    }

    fd = g_file_open(pid_file);

    if (-1 == fd)
    {
      g_printf("error opening pid file[%s]: %s\n", pid_file, g_get_strerror());
      return 1;
    }

    error = g_file_read(fd, pid_s, 7);
    if (-1 == error)
    {
      g_printf("error reading pid file: %s\n", g_get_strerror());
      g_file_close(fd);
      g_deinit();
      g_exit(error);
    }
    g_file_close(fd);
    pid = g_atoi(pid_s);

    error = g_sigterm(pid);
    if (0 != error)
    {
      g_printf("error killing sesman: %s\n", g_get_strerror());
    }
    else
    {
      g_file_delete(pid_file);
    }
    g_deinit();
    g_exit(error);
  }
  else
  {
    /* there's something strange on the command line */
    g_printf("sesman - xrdp session manager\n\n");
    g_printf("error: invalid command line\n");
    g_printf("usage: sesman [ --nodaemon | --kill | --help ]\n");
    g_deinit();
    g_exit(1);
  }

  if (g_file_exist(pid_file))
  {
    g_printf("sesman is already running.\n");
    g_printf("if it's not running, try removing ");
    g_printf(pid_file);
    g_printf("\n");
    g_deinit();
    g_exit(1);
  }

  /* reading config */
  g_cfg = g_malloc(sizeof(struct config_sesman), 1);
  if (0 == g_cfg)
  {
    g_printf("error creating config: quitting.\n");
    g_deinit();
    g_exit(1);
  }
  //g_cfg->log.fd = -1; /* don't use logging before reading its config */
  if (0 != config_read(g_cfg))
  {
    g_printf("error reading config: %s\nquitting.\n", g_get_strerror());
    g_deinit();
    g_exit(1);
  }

  g_snprintf(cfg_file,255,"%s/sesman.ini",XRDP_CFG_PATH);
  
  /* starting logging subsystem */
  error = log_start(cfg_file,"XRDP-sesman");

  if (error != LOG_STARTUP_OK)
  {
    char buf[256] ;  
    switch (error)
    {
      case LOG_ERROR_MALLOC:
        g_printf("error on malloc. cannot start logging. quitting.\n");
        break;
      case LOG_ERROR_FILE_OPEN:
        g_printf("error opening log file [%s]. quitting.\n", getLogFile(buf,255));
        break;
    }
    g_deinit();
    g_exit(1);
  }

  /* libscp initialization */
  scp_init();

  if (daemon)
  {
    /* start of daemonizing code */
    g_pid = g_fork();

    if (0 != g_pid)
    {
      g_deinit();
      g_exit(0);
    }

    g_file_close(0);
    g_file_close(1);
    g_file_close(2);

    g_file_open("/dev/null");
    g_file_open("/dev/null");
    g_file_open("/dev/null");
  }

  /* initializing locks */
  lock_init();

  /* signal handling */
  g_pid = g_getpid();
  /* old style signal handling is now managed synchronously by a
   * separate thread. uncomment this block if you need old style
   * signal handling and comment out thread_sighandler_start()
   * going back to old style for the time being
   * problem with the sigaddset functions in sig.c - jts */
#if 1
  g_signal_hang_up(sig_sesman_reload_cfg); /* SIGHUP  */
  g_signal_user_interrupt(sig_sesman_shutdown); /* SIGINT  */
  g_signal_kill(sig_sesman_shutdown); /* SIGKILL */
  g_signal_terminate(sig_sesman_shutdown); /* SIGTERM */
  g_signal_child_stop(sig_sesman_session_end); /* SIGCHLD */
#endif
#if 0
  thread_sighandler_start();
#endif
  if (daemon)
  {
    /* writing pid file */
    fd = g_file_open(pid_file);
    if (-1 == fd)
    {
      log_message(LOG_LEVEL_ERROR,
                  "error opening pid file[%s]: %s",
                  pid_file, g_get_strerror());
      log_end();
      g_deinit();
      g_exit(1);
    }
    g_sprintf(pid_s, "%d", g_pid);
    g_file_write(fd, pid_s, g_strlen(pid_s));
    g_file_close(fd);
  }

  /* start program main loop */
  log_message(LOG_LEVEL_ALWAYS,
              "starting sesman with pid %d", g_pid);

  /* make sure the /tmp/.X11-unix directory exist */
  if (!g_directory_exist("/tmp/.X11-unix"))
  {
    g_create_dir("/tmp/.X11-unix");
    g_chmod_hex("/tmp/.X11-unix", 0x1777);
  }

  g_snprintf(text, 255, "xrdp_sesman_%8.8x_main_term", g_pid);
  g_term_event = g_create_wait_obj(text);
  g_snprintf(text, 255, "xrdp_sesman_%8.8x_main_sync", g_pid);
  g_sync_event = g_create_wait_obj(text);

  sesman_main_loop();

  /* clean up PID file on exit */
  if (daemon)
  {
    g_file_delete(pid_file);
  }

  g_delete_wait_obj(g_term_event);
  g_delete_wait_obj(g_sync_event);

  if (!daemon)
  {
    log_end();
  }

  g_deinit();
  return 0;
}

