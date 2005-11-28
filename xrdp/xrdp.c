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
   Copyright (C) Jay Sorg 2004-2005

   main program

*/

#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif
#include "xrdp.h"

static struct xrdp_listen* g_listen = 0;
static int g_threadid = 0; /* main threadid */

#if defined(_WIN32)
static SERVICE_STATUS_HANDLE g_ssh = 0;
static SERVICE_STATUS g_service_status;
static CRITICAL_SECTION g_term_mutex;
static CRITICAL_SECTION g_sync_mutex;
static CRITICAL_SECTION g_sync1_mutex;
#define LOCK_ENTER(mutex) EnterCriticalSection(&mutex)
#define LOCK_LEAVE(mutex) LeaveCriticalSection(&mutex)
#else
static pthread_mutex_t g_term_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_sync_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_sync1_mutex = PTHREAD_MUTEX_INITIALIZER;
#define LOCK_ENTER(mutex) pthread_mutex_lock(&mutex)
#define LOCK_LEAVE(mutex) pthread_mutex_unlock(&mutex)
#endif
static int g_term = 0;
/* syncronize stuff */
static int g_sync_command = 0;
static long g_sync_result = 0;
static long g_sync_param1 = 0;
static long g_sync_param2 = 0;
static long (*g_sync_func)(long param1, long param2);

/*****************************************************************************/
long APP_CC
g_xrdp_sync(long (*sync_func)(long param1, long param2), long sync_param1,
            long sync_param2)
{
  long sync_result;
  int sync_command;

  LOCK_ENTER(g_sync1_mutex);
  g_lock();
  g_sync_param1 = sync_param1;
  g_sync_param2 = sync_param2;
  g_sync_func = sync_func;
  g_sync_command = 100;
  g_unlock();
  do
  {
    g_sleep(100);
    g_lock();
    sync_command = g_sync_command;
    sync_result = g_sync_result;
    g_unlock();
  }
  while (sync_command != 0);
  LOCK_LEAVE(g_sync1_mutex);
  return sync_result;
}

/*****************************************************************************/
void DEFAULT_CC
xrdp_shutdown(int sig)
{
  struct xrdp_listen* listen;

  if (g_get_threadid() != g_threadid)
  {
    return;
  }
  g_printf("shutting down\r\n");
  g_printf("signal %d threadid %d\r\n", sig, g_get_threadid());
  listen = g_listen;
  g_listen = 0;
  if (listen != 0)
  {
    g_set_term(1);
    g_sleep(1000);
    xrdp_listen_delete(listen);
  }
  /* delete the xrdp.pid file */
  g_file_delete("./xrdp.pid");
}

/*****************************************************************************/
int APP_CC
g_is_term(void)
{
  int rv;

  LOCK_ENTER(g_term_mutex);
  rv = g_term;
  LOCK_LEAVE(g_term_mutex);
  return rv;
}

/*****************************************************************************/
void APP_CC
g_lock(void)
{
  LOCK_ENTER(g_sync_mutex);
}

/*****************************************************************************/
void APP_CC
g_unlock(void)
{
  LOCK_LEAVE(g_sync_mutex);
}

/*****************************************************************************/
void APP_CC
g_set_term(int in_val)
{
  LOCK_ENTER(g_term_mutex);
  g_term = in_val;
  LOCK_LEAVE(g_term_mutex);
}

/*****************************************************************************/
void DEFAULT_CC
pipe_sig(int sig_num)
{
  /* do nothing */
  g_printf("got SIGPIPE(%d)\r\n", sig_num);
}

/*****************************************************************************/
void APP_CC
g_loop(void)
{
  g_lock();
  if (g_sync_command != 0)
  {
    if (g_sync_func != 0)
    {
      if (g_sync_command == 100)
      {
        g_sync_result = g_sync_func(g_sync_param1, g_sync_param2);
      }
    }
    g_sync_command = 0;
  }
  g_unlock();
#if defined(_WIN32)
  if (g_ssh != 0)
  {
  }
#endif
}

/* win32 service control functions */
#if defined(_WIN32)

/*****************************************************************************/
VOID WINAPI
MyHandler(DWORD fdwControl)
{
  if (g_ssh == 0)
  {
    return;
  }
  if (fdwControl == SERVICE_CONTROL_STOP)
  {
    g_service_status.dwCurrentState = SERVICE_STOP_PENDING;
    g_set_term(1);
  }
  else if (fdwControl == SERVICE_CONTROL_PAUSE)
  {
    /* shouldn't happen */
  }
  else if (fdwControl == SERVICE_CONTROL_CONTINUE)
  {
    /* shouldn't happen */
  }
  else if (fdwControl == SERVICE_CONTROL_INTERROGATE)
  {
  }
  else if (fdwControl == SERVICE_CONTROL_SHUTDOWN)
  {
    g_service_status.dwCurrentState = SERVICE_STOP_PENDING;
    g_set_term(1);
  }
  SetServiceStatus(g_ssh, &g_service_status);
}

/*****************************************************************************/
static void DEFAULT_CC
log_event(HANDLE han, char* msg)
{
  ReportEvent(han, EVENTLOG_INFORMATION_TYPE, 0, 0, 0, 1, 0, &msg, 0);
}

/*****************************************************************************/
VOID WINAPI
MyServiceMain(DWORD dwArgc, LPTSTR* lpszArgv)
{
  WSADATA w;
  //HANDLE event_han;
//  int fd;
//  char text[256];

//  fd = g_file_open("c:\\temp\\xrdp\\log.txt");
//  g_file_write(fd, "hi\r\n", 4);
  //event_han = RegisterEventSource(0, "xrdp");
  //log_event(event_han, "hi xrdp log");
  g_threadid = g_get_threadid();
  g_set_current_dir("c:\\temp\\xrdp");
  g_listen = 0;
  WSAStartup(2, &w);
  InitializeCriticalSection(&g_term_mutex);
  InitializeCriticalSection(&g_sync_mutex);
  InitializeCriticalSection(&g_sync1_mutex);
  g_memset(&g_service_status, 0, sizeof(SERVICE_STATUS));
  g_service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
  g_service_status.dwCurrentState = SERVICE_RUNNING;
  g_service_status.dwControlsAccepted = SERVICE_CONTROL_INTERROGATE |
                                        SERVICE_ACCEPT_STOP |
                                        SERVICE_ACCEPT_SHUTDOWN;
  g_service_status.dwWin32ExitCode = NO_ERROR;
  g_service_status.dwServiceSpecificExitCode = 0;
  g_service_status.dwCheckPoint = 0;
  g_service_status.dwWaitHint = 0;
//  g_sprintf(text, "calling RegisterServiceCtrlHandler\r\n");
//  g_file_write(fd, text, g_strlen(text));
  g_ssh = RegisterServiceCtrlHandler("xrdp", MyHandler);
  if (g_ssh != 0)
  {
//    g_sprintf(text, "ok\r\n");
//    g_file_write(fd, text, g_strlen(text));
    SetServiceStatus(g_ssh, &g_service_status);
    g_listen = xrdp_listen_create();
    xrdp_listen_main_loop(g_listen);
    g_sleep(100);
    g_service_status.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(g_ssh, &g_service_status);
  }
  else
  {
    //g_sprintf(text, "RegisterServiceCtrlHandler failed\r\n");
    //g_file_write(fd, text, g_strlen(text));
  }
  WSACleanup();
  DeleteCriticalSection(&g_term_mutex);
  DeleteCriticalSection(&g_sync_mutex);
  DeleteCriticalSection(&g_sync1_mutex);
  xrdp_listen_delete(g_listen);
  //CloseHandle(event_han);
}

#endif
/*****************************************************************************/
int DEFAULT_CC
main(int argc, char** argv)
{
  int test;
  int host_be;
#if defined(_WIN32)
  WSADATA w;
  SC_HANDLE sc_man;
  SC_HANDLE sc_ser;
  int run_as_service;
  SERVICE_TABLE_ENTRY te[2];
#else
  int pid;
  int fd;
  int no_daemon;
  char text[32];
#endif

  /* check compiled endian with actual edian */
  test = 1;
  host_be = !((int)(*(unsigned char*)(&test)));
#if defined(B_ENDIAN)
  if (!host_be)
#endif
#if defined(L_ENDIAN)
  if (host_be)
#endif
  {
    g_printf("endian wrong, edit arch.h\r\n");
    return 0;
  }
  /* check long, int and void* sizes */
  if (sizeof(int) != 4)
  {
    g_printf("unusable int size, must be 4\r\n");
    return 0;
  }
  if (sizeof(long) != sizeof(void*))
  {
    g_printf("long size must match void* size\r\n");
    return 0;
  }
  if (sizeof(long) != 4 && sizeof(long) != 8)
  {
    g_printf("unusable long size, must be 4 or 8\r\n");
    return 0;
  }
#if defined(_WIN32)
  run_as_service = 1;
  if (argc == 2)
  {
    if (g_strncasecmp(argv[1], "-help", 255) == 0 ||
        g_strncasecmp(argv[1], "--help", 255) == 0 ||
        g_strncasecmp(argv[1], "-h", 255) == 0)
    {
      g_printf("\r\n");
      g_printf("xrdp: A Remote Desktop Protocol server.\r\n");
      g_printf("Copyright (C) Jay Sorg 2004-2005\r\n");
      g_printf("See http://xrdp.sourceforge.net for more information.\r\n");
      g_printf("\r\n");
      g_printf("Usage: xrdp [options]\r\n");
      g_printf("   -h: show help\r\n");
      g_printf("   -install: install service\r\n");
      g_printf("   -remove: remove service\r\n");
      g_printf("\r\n");
      g_exit(0);
    }
    else if (g_strncasecmp(argv[1], "-install", 255) == 0 ||
             g_strncasecmp(argv[1], "--install", 255) == 0 ||
             g_strncasecmp(argv[1], "-i", 255) == 0)
    {
      /* open service manager */
      sc_man = OpenSCManager(0, 0, GENERIC_WRITE);
      if (sc_man == 0)
      {
        g_printf("error OpenSCManager, do you have rights?\r\n");
        g_exit(0);
      }
      /* check if service is allready installed */
      sc_ser = OpenService(sc_man, "xrdp", SERVICE_ALL_ACCESS);
      if (sc_ser == 0)
      {
        /* install service */
        CreateService(sc_man, "xrdp", "xrdp", SERVICE_ALL_ACCESS,
                      SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START,
                      SERVICE_ERROR_IGNORE, "c:\\temp\\xrdp\\xrdp.exe",
                      0, 0, 0, 0, 0);

      }
      else
      {
        g_printf("error service is allready installed\r\n");
        CloseServiceHandle(sc_ser);
        CloseServiceHandle(sc_man);
        g_exit(0);
      }
      CloseServiceHandle(sc_man);
      g_exit(0);
    }
    else if (g_strncasecmp(argv[1], "-remove", 255) == 0 ||
             g_strncasecmp(argv[1], "--remove", 255) == 0 ||
             g_strncasecmp(argv[1], "-r", 255) == 0)
    {
      /* open service manager */
      sc_man = OpenSCManager(0, 0, GENERIC_WRITE);
      if (sc_man == 0)
      {
        g_printf("error OpenSCManager, do you have rights?\r\n");
        g_exit(0);
      }
      /* check if service is allready installed */
      sc_ser = OpenService(sc_man, "xrdp", SERVICE_ALL_ACCESS);
      if (sc_ser == 0)
      {
        g_printf("error service is not installed\r\n");
        CloseServiceHandle(sc_man);
        g_exit(0);
      }
      DeleteService(sc_ser);
      CloseServiceHandle(sc_man);
      g_exit(0);
    }
    else
    {
      g_printf("Unknown Parameter\r\n");
      g_printf("xrdp -h for help\r\n");
      g_printf("\r\n");
      g_exit(0);
    }
  }
  else if (argc > 1)
  {
    g_printf("Unknown Parameter\r\n");
    g_printf("xrdp -h for help\r\n");
    g_printf("\r\n");
    g_exit(0);
  }
  if (run_as_service)
  {
    g_memset(&te, 0, sizeof(te));
    te[0].lpServiceName = "xrdp";
    te[0].lpServiceProc = MyServiceMain;
    StartServiceCtrlDispatcher(&te);
    g_exit(0);
  }
  WSAStartup(2, &w);
  InitializeCriticalSection(&g_term_mutex);
  InitializeCriticalSection(&g_sync_mutex);
  InitializeCriticalSection(&g_sync1_mutex);
#else /* _WIN32 */
  no_daemon = 0;
  if (argc == 2)
  {
    if (g_strncasecmp(argv[1], "-kill", 255) == 0 ||
        g_strncasecmp(argv[1], "--kill", 255) == 0)
    {
      g_printf("stopping xrdp\r\n");
      /* read the xrdp.pid file */
      fd = -1;
      if (g_file_exist("./xrdp.pid"))
      {
        fd = g_file_open("./xrdp.pid");
      }
      if (fd == -1)
      {
        g_printf("problem opening to xrdp.pid\r\n");
        g_printf("maybe its not running\r\n");
      }
      else
      {
        g_memset(text, 0, 32);
        g_file_read(fd, text, 31);
        pid = g_atoi(text);
        g_printf("stopping process id %d\r\n", pid);
        if (pid > 0)
        {
          g_sigterm(pid);
        }
        g_file_close(fd);
      }
      g_exit(0);
    }
    else if (g_strncasecmp(argv[1], "-nodeamon", 255) == 0 ||
             g_strncasecmp(argv[1], "--nodeamon", 255) == 0)
    {
      no_daemon = 1;
    }
    else if (g_strncasecmp(argv[1], "-help", 255) == 0 ||
             g_strncasecmp(argv[1], "--help", 255) == 0 ||
             g_strncasecmp(argv[1], "-h", 255) == 0)
    {
      g_printf("\r\n");
      g_printf("xrdp: A Remote Desktop Protocol server.\r\n");
      g_printf("Copyright (C) Jay Sorg 2004-2005\r\n");
      g_printf("See http://xrdp.sourceforge.net for more information.\r\n");
      g_printf("\r\n");
      g_printf("Usage: xrdp [options]\r\n");
      g_printf("   -h: show help\r\n");
      g_printf("   -nodeamon: don't fork into background\r\n");
      g_printf("   -kill: shut down xrdp\r\n");
      g_printf("\r\n");
      g_exit(0);
    }
    else
    {
      g_printf("Unknown Parameter\r\n");
      g_printf("xrdp -h for help\r\n");
      g_printf("\r\n");
      g_exit(0);
    }
  }
  else if (argc > 1)
  {
    g_printf("Unknown Parameter\r\n");
    g_printf("xrdp -h for help\r\n");
    g_printf("\r\n");
    g_exit(0);
  }
  if (g_file_exist("./xrdp.pid"))
  {
    g_printf("It looks like xrdp is allready running,\r\n");
    g_printf("if not delete the xrdp.pid file and try again\r\n");
    g_exit(0);
  }
  if (!no_daemon)
  {
    /* start of daemonizing code */
    pid = g_fork();
    if (pid == -1)
    {
      g_printf("problem forking\r\n");
      g_exit(1);
    }
    if (0 != pid)
    {
      g_printf("process %d started ok\r\n", pid);
      /* exit, this is the main process */
      g_exit(0);
    }
    g_sleep(1000);
    g_file_close(0);
    g_file_close(1);
    g_file_close(2);
    g_file_open("/dev/null");
    g_file_open("/dev/null");
    g_file_open("/dev/null");
    /* end of daemonizing code */
  }
  /* write the pid to file */
  pid = g_getpid();
  fd = g_file_open("./xrdp.pid");
  if (fd == -1)
  {
    g_printf("trying to write process id to xrdp.pid\r\n");
    g_printf("problem opening xrdp.pid\r\n");
    g_printf("maybe no rights\r\n");
  }
  else
  {
    g_set_file_rights("./xrdp.pid", 1, 1);
    g_sprintf(text, "%d", pid);
    g_file_write(fd, text, g_strlen(text));
    g_file_close(fd);
  }
#endif
  g_threadid = g_get_threadid();
  g_listen = xrdp_listen_create();
  g_signal(2, xrdp_shutdown); /* SIGINT */
  g_signal(9, xrdp_shutdown); /* SIGKILL */
  g_signal(13, pipe_sig); /* sig pipe */
  g_signal(15, xrdp_shutdown); /* SIGTERM */
  xrdp_listen_main_loop(g_listen);
#if defined(_WIN32)
  /* I don't think it ever gets here */
  WSACleanup();
  DeleteCriticalSection(&g_term_mutex);
  DeleteCriticalSection(&g_sync_mutex);
  DeleteCriticalSection(&g_sync1_mutex);
  xrdp_listen_delete(g_listen);
#endif
  return 0;
}

