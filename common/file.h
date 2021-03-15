/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2014
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
 *
 * read a config file
 */

#if !defined(FILE_H)
#define FILE_H

#include "arch.h"

int
file_read_sections(int fd, struct list *names);
int
file_by_name_read_sections(const char *file_name, struct list *names);
int
file_read_section(int fd, const char *section,
                  struct list *names, struct list *values);
int
file_by_name_read_section(const char *file_name, const char *section,
                          struct list *names, struct list *values);

#endif
