
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

#endif
