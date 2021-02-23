/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2012-2013
 * Copyright (C) Thomas Goddard 2012-2013
 * Copyright (C) Laxmikant Rashinkar 2012-2013
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

/*
 * xrdpapi header, do not use os_calls.h, arch.h or any xrdp internal headers
 * this file is included in 3rd party apps
 */

#if !defined(XRDPAPI_H_)
#define XRDPAPI_H_

#ifdef __cplusplus
extern "C" {
#endif

#define WTS_CURRENT_SERVER_HANDLE                   0x00000000
#define WTS_CURRENT_SESSION                         0xffffffff

#define WTS_CHANNEL_OPTION_STATIC                   0x00000000
#define WTS_CHANNEL_OPTION_DYNAMIC                  0x00000001
#define WTS_CHANNEL_OPTION_DYNAMIC_NO_COMPRESS      0x00000001
#define WTS_CHANNEL_OPTION_DYNAMIC_PRI_LOW          0x00000001
#define WTS_CHANNEL_OPTION_DYNAMIC_PRI_MED          0x00000002
#define WTS_CHANNEL_OPTION_DYNAMIC_PRI_HIGH         0x00000003
#define WTS_CHANNEL_OPTION_DYNAMIC_PRI_REAL         0x00000004

typedef enum _WTS_VIRTUAL_CLASS
{
    WTSVirtualClientData,
    WTSVirtualFileHandle
}
WTS_VIRTUAL_CLASS;

/*
 * Reference:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/aa383464(v=vs.85).aspx
 */

void *WTSVirtualChannelOpen(void *hServer, unsigned int SessionId,
                            const char *pVirtualName);

void *WTSVirtualChannelOpenEx(unsigned int SessionId,
                              const char *pVirtualName, unsigned int flags);

int WTSVirtualChannelWrite(void *hChannelHandle, const char *Buffer,
                           unsigned int Length, unsigned int *pBytesWritten);

int WTSVirtualChannelRead(void *hChannelHandle, unsigned int TimeOut,
                          char *Buffer, unsigned int BufferSize,
                          unsigned int *pBytesRead);

int WTSVirtualChannelClose(void *hChannelHandle);

int WTSVirtualChannelQuery(void *hChannelHandle, WTS_VIRTUAL_CLASS WtsVirtualClass,
                           void **ppBuffer, unsigned int *pBytesReturned);

void WTSFreeMemory(void *pMemory);

#ifdef __cplusplus
}
#endif

#endif /* XRDPAPI_H_ */
