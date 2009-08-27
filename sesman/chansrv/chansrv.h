
#if !defined(CHANSRV_H)
#define CHANSRV_H

#include "arch.h"
#include "parse.h"

struct chan_item
{
  int id;
  int flags;
  char name[16];
};

int APP_CC
send_channel_data(int chan_id, char* data, int size);

#define LOG_LEVEL 10

#define LOG(_a, _params) \
{ \
  if (_a < LOG_LEVEL) \
  { \
    g_write("xrdp-chansrv [%10.10d]: ", g_time3()); \
    g_writeln _params ; \
  } \
}

#endif
