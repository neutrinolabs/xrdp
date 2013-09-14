
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

#define PCSC_API

typedef unsigned char BYTE;
typedef BYTE *LPBYTE;
typedef unsigned int LONG;
typedef unsigned int DWORD;
typedef DWORD *LPDWORD;
typedef const void *LPCVOID;
typedef const char *LPCSTR;
typedef char *LPSTR;
typedef void *LPVOID;
typedef const BYTE *LPCBYTE;

typedef LONG SCARDCONTEXT;
typedef SCARDCONTEXT *LPSCARDCONTEXT;

typedef LONG SCARDHANDLE;
typedef SCARDHANDLE *LPSCARDHANDLE;

#define MAX_ATR_SIZE 33

typedef struct _SCARD_READERSTATE
{
    const char *szReader;
    void *pvUserData;
    DWORD dwCurrentState;
    DWORD dwEventState;
    DWORD cbAtr;
    unsigned char rgbAtr[MAX_ATR_SIZE];
} SCARD_READERSTATE, *LPSCARD_READERSTATE;

typedef struct _SCARD_IO_REQUEST
{
    unsigned long dwProtocol;
    unsigned long cbPciLength;
} SCARD_IO_REQUEST, *PSCARD_IO_REQUEST, *LPSCARD_IO_REQUEST;

#define LLOG_LEVEL 11
#define LLOGLN(_level, _args) \
  do { if (_level < LLOG_LEVEL) { printf _args ; printf("\n"); } } while (0)

#define SCARD_S_SUCCESS 0x00000000
#define SCARD_F_INTERNAL_ERROR ((LONG)0x80100001)

static int g_fd = 0; /* unix domain socket */

/*****************************************************************************/
static int
get_display_num_from_display(char *display_text)
{
    int rv;
    int index;
    int mode;
    int host_index;
    int disp_index;
    int scre_index;
    char host[256];
    char disp[256];
    char scre[256];

    memset(host, 0, 256);
    memset(disp, 0, 256);
    memset(scre, 0, 256);

    index = 0;
    host_index = 0;
    disp_index = 0;
    scre_index = 0;
    mode = 0;

    while (display_text[index] != 0)
    {
        if (display_text[index] == ':')
        {
            mode = 1;
        }
        else if (display_text[index] == '.')
        {
            mode = 2;
        }
        else if (mode == 0)
        {
            host[host_index] = display_text[index];
            host_index++;
        }
        else if (mode == 1)
        {
            disp[disp_index] = display_text[index];
            disp_index++;
        }
        else if (mode == 2)
        {
            scre[scre_index] = display_text[index];
            scre_index++;
        }
        index++;
    }
    host[host_index] = 0;
    disp[disp_index] = 0;
    scre[scre_index] = 0;
    rv = atoi(disp);
    return 0;
}

/*****************************************************************************/
PCSC_API LONG
SCardEstablishContext(DWORD dwScope, LPCVOID pvReserved1, LPCVOID pvReserved2,
                      LPSCARDCONTEXT phContext)
{
    int sck;
    int dis;
    char *xrdp_session;
    char *xrdp_display;
    char *home_str;
    char text[256];

    LLOGLN(10, ("SCardEstablishContext:"));
    xrdp_session = getenv("XRDP_SESSION");
    if (xrdp_session == NULL)
    {
        /* XRDP_SESSION must be set */
        return SCARD_F_INTERNAL_ERROR;
    }
    xrdp_display = getenv("DISPLAY");
    if (xrdp_display == NULL)
    {
        /* DISPLAY must be set */
        return SCARD_F_INTERNAL_ERROR;
    }
    home_str = getenv("HOME");
    if (home_str == NULL)
    {
        /* HOME must be set */
        return SCARD_F_INTERNAL_ERROR;
    }
    dis = get_display_num_from_display(xrdp_display);
    if (dis < 10)
    {
        /* DISPLAY must be > 9 */
        return SCARD_F_INTERNAL_ERROR;
    }
    if (g_fd == 0)
    {
        g_fd = socket(PF_LOCAL, SOCK_STREAM, 0);
        snprintf(text, 255, "%s/.pcsc%d", home_str, dis);
    }
    if (g_fd == 0)
    {
        return SCARD_F_INTERNAL_ERROR;
    }
    *phContext = 0;
    return SCARD_S_SUCCESS;
}

/*****************************************************************************/
PCSC_API LONG
SCardReleaseContext(SCARDCONTEXT hContext)
{
    LLOGLN(10, ("SCardReleaseContext:"));
    return SCARD_S_SUCCESS;
}

/*****************************************************************************/
PCSC_API LONG
SCardIsValidContext(SCARDCONTEXT hContext)
{
    LLOGLN(10, ("SCardIsValidContext:"));
    return SCARD_S_SUCCESS;
}

/*****************************************************************************/
PCSC_API LONG
SCardConnect(SCARDCONTEXT hContext, LPCSTR szReader, DWORD dwShareMode,
             DWORD dwPreferredProtocols, LPSCARDHANDLE phCard,
             LPDWORD pdwActiveProtocol)
{
    LLOGLN(10, ("SCardConnect:"));
    return SCARD_S_SUCCESS;
}

/*****************************************************************************/
PCSC_API LONG
SCardReconnect(SCARDHANDLE hCard, DWORD dwShareMode,
               DWORD dwPreferredProtocols, DWORD dwInitialization,
               LPDWORD pdwActiveProtocol)
{
    LLOGLN(10, ("SCardReconnect:"));
    return SCARD_S_SUCCESS;
}

/*****************************************************************************/
PCSC_API LONG
SCardDisconnect(SCARDHANDLE hCard, DWORD dwDisposition)
{
    LLOGLN(10, ("SCardDisconnect:"));
    return SCARD_S_SUCCESS;
}

/*****************************************************************************/
PCSC_API LONG
SCardBeginTransaction(SCARDHANDLE hCard)
{
    LLOGLN(0, ("SCardBeginTransaction:"));
    return SCARD_S_SUCCESS;
}

/*****************************************************************************/
PCSC_API LONG
SCardEndTransaction(SCARDHANDLE hCard, DWORD dwDisposition)
{
    LLOGLN(10, ("SCardEndTransaction:"));
    return SCARD_S_SUCCESS;
}

/*****************************************************************************/
PCSC_API LONG
SCardStatus(SCARDHANDLE hCard, LPSTR mszReaderName, LPDWORD pcchReaderLen,
            LPDWORD pdwState, LPDWORD pdwProtocol, LPBYTE pbAtr,
            LPDWORD pcbAtrLen)
{
    LLOGLN(10, ("SCardStatus:"));
    return SCARD_S_SUCCESS;
}

/*****************************************************************************/
PCSC_API LONG
SCardGetStatusChange(SCARDCONTEXT hContext, DWORD dwTimeout,
                     LPSCARD_READERSTATE rgReaderStates, DWORD cReaders)
{
    LLOGLN(10, ("SCardGetStatusChange:"));
    LLOGLN(10, ("SCardGetStatusChange: dwTimeout %d cReaders %d",
           dwTimeout, cReaders));
    memset(rgReaderStates, 0, sizeof(SCARD_READERSTATE));
    usleep(60000000);
    return SCARD_S_SUCCESS;
}

/*****************************************************************************/
PCSC_API LONG
SCardControl(SCARDHANDLE hCard, DWORD dwControlCode, LPCVOID pbSendBuffer,
             DWORD cbSendLength, LPVOID pbRecvBuffer, DWORD cbRecvLength,
             LPDWORD lpBytesReturned)
{
    LLOGLN(10, ("SCardControl:"));
    return SCARD_S_SUCCESS;
}

/*****************************************************************************/
PCSC_API LONG
SCardTransmit(SCARDHANDLE hCard, const SCARD_IO_REQUEST *pioSendPci,
              LPCBYTE pbSendBuffer, DWORD cbSendLength,
              SCARD_IO_REQUEST *pioRecvPci, LPBYTE pbRecvBuffer,
              LPDWORD pcbRecvLength)
{
    LLOGLN(10, ("SCardTransmit:"));
    return SCARD_S_SUCCESS;
}

/*****************************************************************************/
PCSC_API LONG
SCardListReaderGroups(SCARDCONTEXT hContext, LPSTR mszGroups,
                      LPDWORD pcchGroups)
{
    LLOGLN(10, ("SCardListReaderGroups:"));
    return SCARD_S_SUCCESS;
}

/*****************************************************************************/
PCSC_API LONG
SCardListReaders(SCARDCONTEXT hContext, LPCSTR mszGroups, LPSTR mszReaders,
                 LPDWORD pcchReaders)
{
    LLOGLN(10, ("SCardListReaders:"));
    return SCARD_S_SUCCESS;
}

/*****************************************************************************/
PCSC_API LONG
SCardFreeMemory(SCARDCONTEXT hContext, LPCVOID pvMem)
{
    LLOGLN(10, ("SCardFreeMemory:"));
    return SCARD_S_SUCCESS;
}

/*****************************************************************************/
PCSC_API LONG
SCardCancel(SCARDCONTEXT hContext)
{
    LLOGLN(10, ("SCardCancel:"));
    return SCARD_S_SUCCESS;
}

/*****************************************************************************/
PCSC_API LONG
SCardGetAttrib(SCARDHANDLE hCard, DWORD dwAttrId, LPBYTE pbAttr,
               LPDWORD pcbAttrLen)
{
    LLOGLN(10, ("SCardGetAttrib:"));
    return SCARD_S_SUCCESS;
}

/*****************************************************************************/
PCSC_API LONG
SCardSetAttrib(SCARDHANDLE hCard, DWORD dwAttrId, LPCBYTE pbAttr,
               DWORD cbAttrLen)
{
    LLOGLN(10, ("SCardSetAttrib:"));
    return SCARD_S_SUCCESS;
}
