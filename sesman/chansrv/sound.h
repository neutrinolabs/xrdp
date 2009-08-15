
#if !defined(SOUND_H)
#define SOUND_H

#include "arch.h"
#include "parse.h"

int APP_CC
sound_init(void);
int APP_CC
sound_deinit(void);
int APP_CC
sound_data_in(struct stream* s, int chan_id, int chan_flags, int length,
              int total_length);
int APP_CC
sound_get_wait_objs(tbus* objs, int* count, int* timeout);
int APP_CC
sound_check_wait_objs(void);

#endif
