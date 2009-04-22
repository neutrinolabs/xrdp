
#if !defined(DEVREDIR_H)
#define DEVREDIR_H

#include "arch.h"
#include "parse.h"

int APP_CC
dev_redir_init(void);
int APP_CC
dev_redir_data_in(struct stream* s, int chan_id, int chan_flags, int length,
                  int total_length);
int APP_CC
dev_redir_get_wait_objs(tbus* objs, int* count, int* timeout);
int APP_CC
dev_redir_check_wait_objs(void);

#endif
