/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2009-2012
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "arch.h"
#include "parse.h"
#include "os_calls.h"

extern int g_rdpdr_chan_id; /* in chansrv.c */

/*****************************************************************************/
int APP_CC
dev_redir_init(void)
{
  return 0;
}

/*****************************************************************************/
int APP_CC
dev_redir_deinit(void)
{
  return 0;
}

/*****************************************************************************/
int APP_CC
dev_redir_data_in(struct stream* s, int chan_id, int chan_flags, int length,
                  int total_length)
{
  return 0;
}

/*****************************************************************************/
int APP_CC
dev_redir_get_wait_objs(tbus* objs, int* count, int* timeout)
{
  return 0;
}

/*****************************************************************************/
int APP_CC
dev_redir_check_wait_objs(void)
{
  return 0;
}
