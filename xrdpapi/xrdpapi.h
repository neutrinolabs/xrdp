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

#include <stdint.h>

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

#define WM_WTSSESSION_CHANGE            0x02B1

/*
 * codes passed in WPARAM for WM_WTSSESSION_CHANGE
 * Unlisted codes are not yet implemented.
 */
#define WTS_REMOTE_CONNECT                 0x3
#define WTS_REMOTE_DISCONNECT              0x4

typedef enum _WTS_INFO_CLASS
{
    //WTSInitialProgram,   // Not yet implemented
    //WTSApplicationName,  // Not yet implemented
    //WTSWorkingDirectory, // Not yet implemented
    //WTSOEMId,            // Not yet implemented
    //WTSSessionId,        // Not yet implemented
    //WTSUserName,         // Not yet implemented
    //WTSWinStationName,   // Not yet implemented
    //WTSDomainName,       // Not yet implemented
    WTSConnectState = 8
} WTS_INFO_CLASS;

typedef enum _WTS_CONNECTSTATE_CLASS
{
    // WTSActive,
    WTSConnected = 1,
    // WTSConnectQuery,
    // WTSShadow,
    WTSDisconnected = 4,
    // WTSIdle,
    // WTSListen,
    // WTSReset,
    // WTSDown,
    // WTSInit
} WTS_CONNECTSTATE_CLASS;

typedef enum _WTS_VIRTUAL_CLASS
{
    WTSVirtualClientData,
    WTSVirtualFileHandle
}
WTS_VIRTUAL_CLASS;

// Enumerated type for an error code from some calls. This is not
// compatible with the Windows API.
enum wts_errcode
{
    WTS_E_NO_ERROR = 0,
    // Retryable errors
    WTS_E_CHANSRV_NOT_UP,
    // Fatal errors
    WTS_E_BAD_SERVER = 32,
    WTS_E_BAD_SESSION_ID,
    WTS_E_RESOURCE_ERROR,
    WTS_E_BAD_INFO_CLASS
};

#define WTS_ERRCODE_FATAL(errcode) ((int)(errcode) >= (int)WTS_E_BAD_SERVER)

// Win32 basic types
// See https://learn.microsoft.com/en-us/windows/win32/winprog/windows-data-types
typedef uint32_t DWORD;
typedef uint32_t UINT;
typedef intptr_t UINT_PTR;
typedef intptr_t LONG_PTR;
typedef UINT_PTR WPARAM;
typedef LONG_PTR LPARAM;
typedef LONG_PTR LRESULT;

// WNDPROC emulation for WTSRegisterSessionNotificationEx()
typedef LRESULT WNDPROC(void *cbdata, UINT msg, WPARAM wParam, LPARAM lParam);

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

/**
 * This function is similar to, but not the same as the Win32
 * function of the same name.
 *
 * The purpose of it is to allow an application to find out
 * (rather limited) information about the session
 *
 * @param hServer set to WTS_CURRENT_SERVER_HANDLE
 * @param SessionId current session ID; *must* be WTS_CURRENT_SESSION
 * @param WTSInfoClass parameter to query
 * @param ppBuffer pointer for result
 * @param pBytesReturned size of result
 * @param[out] errcode Status of operation if false returned. Can be NULL.
 * @return true for success
 */
int WTSQuerySessionInformationA(void *hServer,
                                unsigned int SessionId,
                                WTS_INFO_CLASS WTSInfoClass,
                                void           *ppBuffer,
                                DWORD          *pBytesReturned,
                                enum wts_errcode *errcode);

/**
 * This function is similar to, but not the same as the Win32
 * function of the same name.
 *
 * The purpose of it is to allow an application to receive
 * WM_WTSSESSION_CHANGE messages.
 *
 * @param hServer set to WTS_CURRENT_SERVER_HANDLE
 * @param[out] fd_ptr File descriptor to check for notification messages
 * @param dwFlags ignored
 * @param[out] errcode Status of operation if false returned. Can be NULL.
 * @return true for success
 *
 * The fd_ptr replaces the hWnd parameter in the Win32 call.
 *
 * After a successful call, the location pointed-to by fd_ptr will contain a
 * file descriptor which the caller can poll for session change messages.
 * When the file descriptor becomes readable, a call to
 * WTSGetDispatchMessage() will process a single session change message.
 *
 * The caller must do nothing with the returned file descriptor except poll it.
 */
int WTSRegisterSessionNotificationEx(void *hServer,
                                     int *fd_ptr,
                                     int dwFlags,
                                     enum wts_errcode *errcode);

/**
 * This function is similar to, but not the same as the Win32
 * function of the same name.
 *
 * The purpose of it is to deallocate resources associated with
 * WTSRegisterSessionNotificationEx()
 *
 * @param hServer set to WTS_CURRENT_SERVER_HANDLE
 * @param fd File descriptor from WTSRegisterSessionNotificationEx
 * @param[out] errcode Status of operation if false returned. Can be NULL.
 * @return true for success
 */
int WTSUnRegisterSessionNotificationEx(void *hServer,
                                       int fd,
                                       enum wts_errcode *errcode);


/** Replaces Win32 GetMessage() / DispatchMessage()
 *
 * @param cbdata callback data to pass in to WNDPROC
 * @param wndproc WNDPROC to call
 * @param[out] lResult Result of WNDPROC if call is successful
 * @return != 0 if a WNDPROC was successfully called
 */
int
WTSGetDispatchMessage(void *cbdata, WNDPROC wndproc, LRESULT *lResult);

#ifdef __cplusplus
}
#endif

#endif /* XRDPAPI_H_ */
