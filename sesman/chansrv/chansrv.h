
#if !defined(CHANSRV_H)
#define CHANSRV_H

#include "arch.h"
#include "parse.h"

#define XRDP_CHANNEL_LOG 0

struct chan_item
{
  int id;
  int flags;
  char name[16];
};

int APP_CC
send_channel_data(int chan_id, char* data, int size);
int APP_CC
main_cleanup(void);

#define LOG_LEVEL 5

#define LOG(_a, _params) \
{ \
  if (_a < LOG_LEVEL) \
  { \
    g_write("xrdp-chansrv [%10.10u]: ", g_time3()); \
    g_writeln _params ; \
  } \
}

#if XRDP_CHANNEL_LOG
#include "log.h"
#define LOGM(_args) do { log_message _args ; } while (0)
#else
#define LOGM(_args)
#endif

#endif
