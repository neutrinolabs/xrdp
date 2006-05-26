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
   Copyright (C) Jay Sorg 2005-2006
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
#ifdef OLDSESSION
struct session_item g_session_items[100]; /* sesman.h */
extern int g_session_count;
#endif
struct config_sesman g_cfg; /* config.h */
//int g_server_type = 0; /* Xvnc 0 Xrdp 10 */

/**
 *
 * @brief triggered when a child process (a session) dies
 * @param s received signal
 *
 */
static void DEFAULT_CC
cterm(int s)
{
#ifdef OLDSESSION
  int i;
#endif
  int pid;

  if (g_getpid() != g_pid)
  {
    return;
  }
  pid = g_waitchild();
  if (pid > 0)
  {
#ifdef OLDSESSION
    for (i = 0; i < 100; i++)
    {
      if (g_session_items[i].pid == pid)
      {
        g_memset(g_session_items + i, 0, sizeof(struct session_item));
	g_session_count--;
      }
    }
#else
    session_kill(pid);
#endif
  }
}

/******************************************************************************/
/**
 *
 * @brief Starts sesman main loop
 *
 */
static void DEFAULT_CC 
sesman_main_loop()
{
  int in_sck;
  int error;

  /*main program loop*/
  log_message(LOG_LEVEL_INFO, "listening...");
  g_sck = g_tcp_socket();
  g_tcp_set_non_blocking(g_sck);
  error = g_tcp_bind(g_sck, g_cfg.listen_port);
  if (error == 0)
  {
    error = g_tcp_listen(g_sck);
    if (error == 0)
    {
      in_sck = g_tcp_accept(g_sck);
      while (in_sck == -1 && g_tcp_last_error_would_block(g_sck))
      {
        g_sleep(1000);
        in_sck = g_tcp_accept(g_sck);
      }
      while (in_sck > 0)
      {
        /* we've got a connection, so we pass it to scp code */
        scp_process_start(in_sck);

        /* once we've processed the connection, we go back listening */
        in_sck = g_tcp_accept(g_sck);
        while (in_sck == -1 && g_tcp_last_error_would_block(g_sck))
        {
          g_sleep(1000);
          in_sck = g_tcp_accept(g_sck);
        }
      }
    }
    else
    {
      log_message(LOG_LEVEL_ERROR, "listen error");
    }
  }
  else
  {
    log_message(LOG_LEVEL_ERROR, "bind error");
  }
  g_tcp_close(g_sck);
}

/******************************************************************************/
int DEFAULT_CC
main(int argc, char** argv)
{
  int fd;
  int error;
  int daemon = 1;
  int pid;
  char pid_s[8];

  if (1 == argc)
  {
    /* no options on command line. normal startup */
    g_printf("starting sesman...\n");
    daemon = 1;
  }
  else if ((2 == argc) && ((0 == g_strcasecmp(argv[1], "--nodaemon")) ||
                           (0 == g_strcasecmp(argv[1], "-n")) ) )
  {
    /* starts sesman not daemonized */
    g_printf("starting sesman in foregroud...\n");
    daemon=0;
  }
  else if ((2 == argc) && ((0 == g_strcasecmp(argv[1], "--help")) ||
                           (0 == g_strcasecmp(argv[1], "-h"))))
  {
    /* help screen */
    g_printf("sesman - xrdp session manager\n\n");
    g_printf("usage: sesman [command]\n\n");
    g_printf("command can be one of the following:\n");
    g_printf("-n, --nodaemon  starts sesman in foregroun\n");
    g_printf("-k, --kill      kills running sesman\n");
    g_printf("-h, --help      shows this help\n");
    g_printf("if no command is specified, sesman is started in background");
    g_exit(0);
  }
  else if ((2 == argc) && ((0 == g_strcasecmp(argv[1], "--kill")) ||
                           (0 == g_strcasecmp(argv[1], "-k"))))
  {
    /* killing running sesman */
    /* check if sesman is running */
    if (!g_file_exist(SESMAN_PID_FILE))
    {
      g_printf("sesman is not running (pid file not found - %s)\n",
               SESMAN_PID_FILE);
      g_exit(1);
    }

    fd = g_file_open(SESMAN_PID_FILE);

    if (-1 == fd)
    {
      g_printf("error opening pid file: %s\n", g_get_strerror());
      return 1;
    }

    error = g_file_read(fd, pid_s, 7);
    pid = g_atoi(pid_s);

    error = g_sigterm(pid);
    if (0 != error)
    {
      g_printf("error killing sesman: %s\n", g_get_strerror());
    }
    else
    {
      g_file_delete(SESMAN_PID_FILE);
    }

    g_exit(error);
  }
  else
  {
    /* there's something strange on the command line */
    g_printf("sesman - xrdp session manager\n\n");
    g_printf("error: invalid command line\n");
    g_printf("usage: sesman [ --nodaemon | --kill | --help ]\n");
    g_exit(1);
  }

  if (g_file_exist(SESMAN_PID_FILE))
  {
    g_printf("sesman is already running.\n");
    g_printf("if it's not running, try removing ");
    g_printf(SESMAN_PID_FILE);
    g_printf("\n");
    g_exit(1);
  }

  /* reading config */
  if (0 != config_read(&g_cfg))
  {
    g_printf("error reading config: %s\nquitting.\n", g_get_strerror());
    g_exit(1);
  }

  /* starting logging subsystem */
  error = log_start(g_cfg.log.program_name, g_cfg.log.log_file,
                    g_cfg.log.log_level, g_cfg.log.enable_syslog,
                    g_cfg.log.syslog_level);

  if (error != LOG_STARTUP_OK)
  {
    switch (error)
    {
      case LOG_ERROR_MALLOC:
        g_printf("error on malloc. cannot start logging. quitting.\n");
        break;
      case LOG_ERROR_FILE_OPEN:
        g_printf("error opening log file. quitting.\n");
        break;
    }
    g_exit(1);
  }
  
  if (daemon)
  {
    /* start of daemonizing code */
    g_pid = g_fork();

    if (0!=g_pid)
    {
      g_exit(0);
    }
  
    g_file_close(0);
    g_file_close(1);
    g_file_close(2);

    g_file_open("/dev/null");
    g_file_open("/dev/null");
    g_file_open("/dev/null");
  }
  
  /* signal handling */
#ifdef OLDSESSION
  g_memset(&g_session_items, 0, sizeof(g_session_items));
#endif
  g_pid = g_getpid();
  g_signal(1, sig_sesman_reload_cfg); /* SIGHUP  */
  g_signal(2, sig_sesman_shutdown);   /* SIGINT  */
  g_signal(9, sig_sesman_shutdown);   /* SIGKILL */
  g_signal(15, sig_sesman_shutdown);  /* SIGTERM */
  g_signal_child_stop(cterm);         /* SIGCHLD */

  /* writing pid file */
  fd = g_file_open(SESMAN_PID_FILE);
  if (-1 == fd)
  {
    log_message(LOG_LEVEL_ERROR, "error opening pid file: %s",
                g_get_strerror());
    log_end();
    g_exit(1);
  }
  g_sprintf(pid_s, "%d", g_pid);
  g_file_write(fd, pid_s, g_strlen(pid_s)+1);
  g_file_close(fd);

  /* start program main loop */
  log_message(LOG_LEVEL_ALWAYS, "starting sesman with pid %d", g_pid);

  sesman_main_loop();

  if (!daemon)
  {
    log_end();
  }

  return 0;
}

