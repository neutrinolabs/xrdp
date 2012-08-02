/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Thomas Goddard 2012
 * Copyright (C) Jay Sorg 2012
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xrdpapi.h"

/*****************************************************************************/
void*
WTSVirtualChannelOpen(void* hServer, unsigned int SessionId,
                      const char* pVirtualName)
{
  return 0;
}

/*****************************************************************************/
void*
WTSVirtualChannelOpenEx(unsigned int SessionId,
                        const char* pVirtualName,
                        unsigned int flags)
{
  return 0;
}

/*****************************************************************************/
int
WTSVirtualChannelWrite(void* hChannelHandle, const char* Buffer,
                       unsigned int Length, unsigned int* pBytesWritten)
{
  return 0;
}

/*****************************************************************************/
int
WTSVirtualChannelRead(void* nChannelHandle, unsigned int TimeOut,
                      char* Buffer, unsigned int BufferSize,
                      unsigned int* pBytesRead)
{
  return 0;
}

/*****************************************************************************/
int
WTSVirtualChannelClose(void* openHandle)
{
  return 0;
}
