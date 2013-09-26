
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>

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

#define LLOG_LEVEL 5
#define LLOGLN(_level, _args) \
  do { if (_level < LLOG_LEVEL) { printf _args ; printf("\n"); } } while (0)

#define SCARD_ESTABLISH_CONTEXT  0x01
#define SCARD_RELEASE_CONTEXT    0x02
#define SCARD_LIST_READERS       0x03
#define SCARD_CONNECT            0x04
#define SCARD_RECONNECT          0x05
#define SCARD_DISCONNECT         0x06
#define SCARD_BEGIN_TRANSACTION  0x07
#define SCARD_END_TRANSACTION    0x08
#define SCARD_TRANSMIT           0x09
#define SCARD_CONTROL            0x0A
#define SCARD_STATUS             0x0B
#define SCARD_GET_STATUS_CHANGE  0x0C
#define SCARD_CANCEL             0x0D
#define SCARD_CANCEL_TRANSACTION 0x0E
#define SCARD_GET_ATTRIB         0x0F
#define SCARD_SET_ATTRIB         0x10

#define SCARD_S_SUCCESS 0x00000000
#define SCARD_F_INTERNAL_ERROR ((LONG)0x80100001)

#define SET_UINT32(_data, _offset, _val) do { \
  (((BYTE*)(_data)) + (_offset))[0] = ((_val) >> 0)  & 0xff; \
  (((BYTE*)(_data)) + (_offset))[1] = ((_val) >> 8)  & 0xff; \
  (((BYTE*)(_data)) + (_offset))[2] = ((_val) >> 16) & 0xff; \
  (((BYTE*)(_data)) + (_offset))[3] = ((_val) >> 24) & 0xff; } while (0)

#define GET_UINT32(_data, _offset) \
  ((((BYTE*)(_data)) + (_offset))[0] << 0)  | \
  ((((BYTE*)(_data)) + (_offset))[1] << 8)  | \
  ((((BYTE*)(_data)) + (_offset))[2] << 16) | \
  ((((BYTE*)(_data)) + (_offset))[3] << 24)

#define LMIN(_val1, _val2) (_val1) < (_val2) ? (_val1) : (_val2)
#define LMAX(_val1, _val2) (_val1) > (_val2) ? (_val1) : (_val2)

static int g_sck = -1; /* unix domain socket */

static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

/* for pcsc_stringify_error */
static char g_error_str[512];

/*****************************************************************************/
static int
get_display_num_from_display(const char *display_text)
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
    LLOGLN(0, ("get_display_num_from_display: host [%s] disp [%s] scre [%s]",
           host, disp, scre));
    rv = atoi(disp);
    return rv;
}

/*****************************************************************************/
static int
connect_to_chansrv(void)
{
    int bytes;
    int dis;
    int error;
    char *xrdp_session;
    char *xrdp_display;
    char *home_str;
    struct sockaddr_un saddr;
    struct sockaddr *psaddr;

    if (g_sck != -1)
    {
        /* already connected */
        return 0;
    }
    xrdp_session = getenv("XRDP_SESSION");
    if (xrdp_session == NULL)
    {
        /* XRDP_SESSION must be set */
        LLOGLN(0, ("connect_to_chansrv: error, not xrdp session"));
        return 1;
    }
    xrdp_display = getenv("DISPLAY");
    if (xrdp_display == NULL)
    {
        /* DISPLAY must be set */
        LLOGLN(0, ("connect_to_chansrv: error, display not set"));
        return 1;
    }
    home_str = getenv("HOME");
    if (home_str == NULL)
    {
        /* HOME must be set */
        LLOGLN(0, ("connect_to_chansrv: error, home not set"));
        return 1;
    }
    dis = get_display_num_from_display(xrdp_display);
    if (dis < 10)
    {
        /* DISPLAY must be > 9 */
        LLOGLN(0, ("connect_to_chansrv: error, display not > 9 %d", dis));
        return 1;
    }
    g_sck = socket(PF_LOCAL, SOCK_STREAM, 0);
    if (g_sck == -1)
    {
        LLOGLN(0, ("connect_to_chansrv: error, socket failed"));
        return 1;
    }
    memset(&saddr, 0, sizeof(struct sockaddr_un));
    saddr.sun_family = AF_UNIX;
    bytes = sizeof(saddr.sun_path);
    snprintf(saddr.sun_path, bytes, "%s/.pcsc%d/pcscd.comm", home_str, dis);
    saddr.sun_path[bytes - 1] = 0;
    LLOGLN(0, ("connect_to_chansrv: connecting to %s", saddr.sun_path));
    psaddr = (struct sockaddr *) &saddr;
    bytes = sizeof(struct sockaddr_un);
    error = connect(g_sck, psaddr, bytes);
    if (error == 0)
    {
    }
    else
    {
        perror("connect_to_chansrv");
        close(g_sck);
        g_sck = -1;
        LLOGLN(0, ("connect_to_chansrv: error, open %s", saddr.sun_path));
        return 1;
    }
    return 0;
}

/*****************************************************************************/
static int
send_message(int code, char *data, int bytes)
{
    char header[8];

    SET_UINT32(header, 0, bytes);
    SET_UINT32(header, 4, code);
    if (send(g_sck, header, 8, 0) != 8)
    {
        return 1;
    }
    if (send(g_sck, data, bytes, 0) != bytes)
    {
        return 1;
    }
    return 0;
}

/*****************************************************************************/
static int
get_message(int *code, char *data, int *bytes)
{
    char header[8];
    int max_bytes;

    if (recv(g_sck, header, 8, 0) != 8)
    {
        return 1;
    }
    max_bytes = *bytes;
    *bytes = GET_UINT32(header, 0);
    *code = GET_UINT32(header, 4);
    if (*bytes > max_bytes)
    {
        return 1;
    }
    if (recv(g_sck, data, *bytes, 0) != *bytes)
    {
        return 1;
    }
    return 0;
}

/*****************************************************************************/
PCSC_API LONG
SCardEstablishContext(DWORD dwScope, LPCVOID pvReserved1, LPCVOID pvReserved2,
                      LPSCARDCONTEXT phContext)
{
    char msg[256];
    DWORD context;
    int code;
    int bytes;
    int status;

    LLOGLN(0, ("SCardEstablishContext:"));
    if (g_sck == -1)
    {
        if (connect_to_chansrv() != 0)
        {
            LLOGLN(0, ("SCardEstablishContext: error, can not connect "
                   "to chansrv"));
            return SCARD_F_INTERNAL_ERROR;
        }
    }
    pthread_mutex_lock(&g_mutex);
    SET_UINT32(msg, 0, dwScope);
    if (send_message(SCARD_ESTABLISH_CONTEXT, msg, 4) != 0)
    {
        LLOGLN(0, ("SCardEstablishContext: error, send_message"));
        pthread_mutex_unlock(&g_mutex);
        return SCARD_F_INTERNAL_ERROR;
    }
    bytes = 256;
    if (get_message(&code, msg, &bytes) != 0)
    {
        LLOGLN(0, ("SCardEstablishContext: error, get_message"));
        pthread_mutex_unlock(&g_mutex);
        return SCARD_F_INTERNAL_ERROR;
    }
    if ((code != SCARD_ESTABLISH_CONTEXT) || (bytes != 8))
    {
        LLOGLN(0, ("SCardEstablishContext: error, bad code"));
        pthread_mutex_unlock(&g_mutex);
        return SCARD_F_INTERNAL_ERROR;
    }
    pthread_mutex_unlock(&g_mutex);
    context = GET_UINT32(msg, 0);
    status = GET_UINT32(msg, 4);
    LLOGLN(10, ("SCardEstablishContext: got context 0x%8.8x", context));
    *phContext = context;
    return status;
}

/*****************************************************************************/
PCSC_API LONG
SCardReleaseContext(SCARDCONTEXT hContext)
{
    char msg[256];
    int code;
    int bytes;
    int status;

    LLOGLN(0, ("SCardReleaseContext:"));
    if (g_sck == -1)
    {
        LLOGLN(0, ("SCardReleaseContext: error, not connected"));
        return SCARD_F_INTERNAL_ERROR;
    }
    pthread_mutex_lock(&g_mutex);
    SET_UINT32(msg, 0, hContext);
    if (send_message(SCARD_RELEASE_CONTEXT, msg, 4) != 0)
    {
        LLOGLN(0, ("SCardReleaseContext: error, send_message"));
        pthread_mutex_unlock(&g_mutex);
        return SCARD_F_INTERNAL_ERROR;
    }
    bytes = 256;
    if (get_message(&code, msg, &bytes) != 0)
    {
        LLOGLN(0, ("SCardReleaseContext: error, get_message"));
        pthread_mutex_unlock(&g_mutex);
        return SCARD_F_INTERNAL_ERROR;
    }
    if ((code != SCARD_RELEASE_CONTEXT) || (bytes != 4))
    {
        LLOGLN(0, ("SCardReleaseContext: error, bad code"));
        pthread_mutex_unlock(&g_mutex);
        return SCARD_F_INTERNAL_ERROR;
    }
    pthread_mutex_unlock(&g_mutex);
    status = GET_UINT32(msg, 0);
    LLOGLN(10, ("SCardReleaseContext: got status 0x%8.8x", status));
    return status;
}

/*****************************************************************************/
PCSC_API LONG
SCardIsValidContext(SCARDCONTEXT hContext)
{
    LLOGLN(0, ("SCardIsValidContext:"));
    if (g_sck == -1)
    {
        LLOGLN(0, ("SCardIsValidContext: error, not connected"));
        return SCARD_F_INTERNAL_ERROR;
    }
    pthread_mutex_lock(&g_mutex);
    pthread_mutex_unlock(&g_mutex);
    return SCARD_S_SUCCESS;
}

/*****************************************************************************/
PCSC_API LONG
SCardConnect(SCARDCONTEXT hContext, LPCSTR szReader, DWORD dwShareMode,
             DWORD dwPreferredProtocols, LPSCARDHANDLE phCard,
             LPDWORD pdwActiveProtocol)
{
    char msg[256];
    int code;
    int bytes;
    int status;
    int offset;

    LLOGLN(0, ("SCardConnect:"));
    if (g_sck == -1)
    {
        LLOGLN(0, ("SCardConnect: error, not connected"));
        return SCARD_F_INTERNAL_ERROR;
    }
    pthread_mutex_lock(&g_mutex);
    offset = 0;
    SET_UINT32(msg, offset, hContext);
    offset += 4;
    bytes = strlen(szReader);
    if (bytes > 99)
    {
        LLOGLN(0, ("SCardConnect: error, name too long"));
        return SCARD_F_INTERNAL_ERROR;
    }
    memcpy(msg + offset, szReader, bytes);
    memset(msg + bytes, 0, 100 - bytes);
    offset += 100;
    SET_UINT32(msg, offset, dwShareMode);
    offset += 4;
    SET_UINT32(msg, offset, dwPreferredProtocols);
    offset += 4;
    if (send_message(SCARD_CONNECT, msg, offset) != 0)
    {
        LLOGLN(0, ("SCardConnect: error, send_message"));
        pthread_mutex_unlock(&g_mutex);
        return SCARD_F_INTERNAL_ERROR;
    }
    bytes = 256;
    if (get_message(&code, msg, &bytes) != 0)
    {
        LLOGLN(0, ("SCardConnect: error, get_message"));
        pthread_mutex_unlock(&g_mutex);
        return SCARD_F_INTERNAL_ERROR;
    }
    if (code != SCARD_CONNECT)
    {
        LLOGLN(0, ("SCardConnect: error, bad code"));
        pthread_mutex_unlock(&g_mutex);
        return SCARD_F_INTERNAL_ERROR;
    }
    pthread_mutex_unlock(&g_mutex);
    *phCard = GET_UINT32(msg, 0);
    *pdwActiveProtocol = GET_UINT32(msg, 4);
    status = GET_UINT32(msg, 8);
    LLOGLN(10, ("SCardReleaseContext: got status 0x%8.8x", status));
    return status;
}

/*****************************************************************************/
PCSC_API LONG
SCardReconnect(SCARDHANDLE hCard, DWORD dwShareMode,
               DWORD dwPreferredProtocols, DWORD dwInitialization,
               LPDWORD pdwActiveProtocol)
{
    LLOGLN(0, ("SCardReconnect:"));
    if (g_sck == -1)
    {
        LLOGLN(0, ("SCardReconnect: error, not connected"));
        return SCARD_F_INTERNAL_ERROR;
    }
    pthread_mutex_lock(&g_mutex);
    pthread_mutex_unlock(&g_mutex);
    return SCARD_S_SUCCESS;
}

/*****************************************************************************/
PCSC_API LONG
SCardDisconnect(SCARDHANDLE hCard, DWORD dwDisposition)
{
    LLOGLN(0, ("SCardDisconnect:"));
    if (g_sck == -1)
    {
        LLOGLN(0, ("SCardDisconnect: error, not connected"));
        return SCARD_F_INTERNAL_ERROR;
    }
    pthread_mutex_lock(&g_mutex);
    pthread_mutex_unlock(&g_mutex);
    return SCARD_S_SUCCESS;
}

/*****************************************************************************/
PCSC_API LONG
SCardBeginTransaction(SCARDHANDLE hCard)
{
    char msg[256];
    int code;
    int bytes;
    int status;

    LLOGLN(0, ("SCardBeginTransaction:"));
    if (g_sck == -1)
    {
        LLOGLN(0, ("SCardBeginTransaction: error, not connected"));
        return SCARD_F_INTERNAL_ERROR;
    }
    pthread_mutex_lock(&g_mutex);
    SET_UINT32(msg, 0, hCard);
    if (send_message(SCARD_BEGIN_TRANSACTION, msg, 4) != 0)
    {
        LLOGLN(0, ("SCardBeginTransaction: error, send_message"));
        pthread_mutex_unlock(&g_mutex);
        return SCARD_F_INTERNAL_ERROR;
    }
    bytes = 256;
    if (get_message(&code, msg, &bytes) != 0)
    {
        LLOGLN(0, ("SCardBeginTransaction: error, get_message"));
        pthread_mutex_unlock(&g_mutex);
        return SCARD_F_INTERNAL_ERROR;
    }
    if ((code != SCARD_BEGIN_TRANSACTION) || (bytes != 4))
    {
        LLOGLN(0, ("SCardBeginTransaction: error, bad code"));
        pthread_mutex_unlock(&g_mutex);
        return SCARD_F_INTERNAL_ERROR;
    }
    pthread_mutex_unlock(&g_mutex);
    status = GET_UINT32(msg, 0);
    LLOGLN(10, ("SCardBeginTransaction: got status 0x%8.8x", status));
    return status;
}

/*****************************************************************************/
PCSC_API LONG
SCardEndTransaction(SCARDHANDLE hCard, DWORD dwDisposition)
{
    LLOGLN(0, ("SCardEndTransaction:"));
    if (g_sck == -1)
    {
        LLOGLN(0, ("SCardEndTransaction: error, not connected"));
        return SCARD_F_INTERNAL_ERROR;
    }
    pthread_mutex_lock(&g_mutex);
    pthread_mutex_unlock(&g_mutex);
    return SCARD_S_SUCCESS;
}

/*****************************************************************************/
PCSC_API LONG
SCardStatus(SCARDHANDLE hCard, LPSTR mszReaderName, LPDWORD pcchReaderLen,
            LPDWORD pdwState, LPDWORD pdwProtocol, LPBYTE pbAtr,
            LPDWORD pcbAtrLen)
{
    LLOGLN(0, ("SCardStatus:"));
    if (g_sck == -1)
    {
        LLOGLN(0, ("SCardStatus: error, not connected"));
        return SCARD_F_INTERNAL_ERROR;
    }
    pthread_mutex_lock(&g_mutex);
    pthread_mutex_unlock(&g_mutex);
    return SCARD_S_SUCCESS;
}

/*****************************************************************************/
PCSC_API LONG
SCardGetStatusChange(SCARDCONTEXT hContext, DWORD dwTimeout,
                     LPSCARD_READERSTATE rgReaderStates, DWORD cReaders)
{
    char *msg;
    int bytes;
    int code;
    int index;
    int offset;
    int str_len;
    int status;

    LLOGLN(0, ("SCardGetStatusChange:"));
    LLOGLN(10, ("  dwTimeout %d cReaders %d", dwTimeout, cReaders));
    if (g_sck == -1)
    {
        LLOGLN(0, ("SCardGetStatusChange: error, not connected"));
        return SCARD_F_INTERNAL_ERROR;
    }
    msg = (char *) malloc(8192);
    SET_UINT32(msg, 0, hContext);
    SET_UINT32(msg, 4, dwTimeout);
    SET_UINT32(msg, 8, cReaders);
    offset = 12;
    for (index = 0; index < cReaders; index++)
    {
        str_len = strlen(rgReaderStates[index].szReader);
        str_len = LMIN(str_len, 99);
        memset(msg + offset, 0, 100);
        memcpy(msg + offset, rgReaderStates[index].szReader, str_len);
        offset += 100;
        LLOGLN(10, ("  in dwCurrentState %d", rgReaderStates[index].dwCurrentState));
        SET_UINT32(msg, offset, rgReaderStates[index].dwCurrentState);
        offset += 4;
        LLOGLN(10, ("  in dwEventState   %d", rgReaderStates[index].dwEventState));
        SET_UINT32(msg, offset, rgReaderStates[index].dwEventState);
        offset += 4;
        LLOGLN(10, ("  in cbAtr          %d", rgReaderStates[index].cbAtr));
        SET_UINT32(msg, offset, rgReaderStates[index].cbAtr);
        offset += 4;
        memset(msg + offset, 0, 36);
        memcpy(msg + offset, rgReaderStates[index].rgbAtr, 33);
        offset += 36;
    }
    pthread_mutex_lock(&g_mutex);
    if (send_message(SCARD_GET_STATUS_CHANGE, msg, offset) != 0)
    {
        LLOGLN(0, ("SCardGetStatusChange: error, send_message"));
        free(msg);
        pthread_mutex_unlock(&g_mutex);
        return SCARD_F_INTERNAL_ERROR;
    }
    bytes = 8192;
    if (get_message(&code, msg, &bytes) != 0)
    {
        LLOGLN(0, ("SCardGetStatusChange: error, get_message"));
        free(msg);
        pthread_mutex_unlock(&g_mutex);
        return SCARD_F_INTERNAL_ERROR;
    }
    if (code != SCARD_GET_STATUS_CHANGE)
    {
        LLOGLN(0, ("SCardGetStatusChange: error, bad code"));
        free(msg);
        pthread_mutex_unlock(&g_mutex);
        return SCARD_F_INTERNAL_ERROR;
    }
    pthread_mutex_unlock(&g_mutex);
    cReaders = GET_UINT32(msg, 0);
    offset = 4;
    LLOGLN(10, ("SCardGetStatusChange: got back cReaders %d", cReaders));
    for (index = 0; index < cReaders; index++)
    {
        rgReaderStates[index].dwCurrentState = GET_UINT32(msg, offset);
        offset += 4;
        LLOGLN(10, ("  out dwCurrentState %d", rgReaderStates[index].dwCurrentState));
        rgReaderStates[index].dwEventState = GET_UINT32(msg, offset);
        offset += 4;
        LLOGLN(10, ("  out dwEventState   %d", rgReaderStates[index].dwEventState));
        rgReaderStates[index].cbAtr = GET_UINT32(msg, offset);
        offset += 4;
        LLOGLN(10, ("  out cbAtr          %d", rgReaderStates[index].cbAtr));
        memcpy(rgReaderStates[index].rgbAtr, msg + offset, 33);
        offset += 36;
    }
    status = GET_UINT32(msg, offset);
    offset += 4;
    free(msg);
    return status;
}

/*****************************************************************************/
PCSC_API LONG
SCardControl(SCARDHANDLE hCard, DWORD dwControlCode, LPCVOID pbSendBuffer,
             DWORD cbSendLength, LPVOID pbRecvBuffer, DWORD cbRecvLength,
             LPDWORD lpBytesReturned)
{
    LLOGLN(0, ("SCardControl:"));
    if (g_sck == -1)
    {
        LLOGLN(0, ("SCardControl: error, not connected"));
        return SCARD_F_INTERNAL_ERROR;
    }
    pthread_mutex_lock(&g_mutex);
    pthread_mutex_unlock(&g_mutex);
    return SCARD_S_SUCCESS;
}

/*****************************************************************************/
PCSC_API LONG
SCardTransmit(SCARDHANDLE hCard, const SCARD_IO_REQUEST *pioSendPci,
              LPCBYTE pbSendBuffer, DWORD cbSendLength,
              SCARD_IO_REQUEST *pioRecvPci, LPBYTE pbRecvBuffer,
              LPDWORD pcbRecvLength)
{
    LLOGLN(0, ("SCardTransmit:"));
    if (g_sck == -1)
    {
        LLOGLN(0, ("SCardTransmit: error, not connected"));
        return SCARD_F_INTERNAL_ERROR;
    }
    pthread_mutex_lock(&g_mutex);
    pthread_mutex_unlock(&g_mutex);
    return SCARD_S_SUCCESS;
}

/*****************************************************************************/
PCSC_API LONG
SCardListReaderGroups(SCARDCONTEXT hContext, LPSTR mszGroups,
                      LPDWORD pcchGroups)
{
    LLOGLN(0, ("SCardListReaderGroups:"));
    if (g_sck == -1)
    {
        LLOGLN(0, ("SCardListReaderGroups: error, not connected"));
        return SCARD_F_INTERNAL_ERROR;
    }
    pthread_mutex_lock(&g_mutex);
    pthread_mutex_unlock(&g_mutex);
    return SCARD_S_SUCCESS;
}

/*****************************************************************************/
PCSC_API LONG
SCardListReaders(SCARDCONTEXT hContext, LPCSTR mszGroups, LPSTR mszReaders,
                 LPDWORD pcchReaders)
{
    char* msg;
    char* reader_names;
    int reader_names_index;
    int code;
    int bytes;
    int num_readers;
    int status;
    int offset;
    int index;
    char reader[100];

    LLOGLN(0, ("SCardListReaders:"));
    if (g_sck == -1)
    {
        LLOGLN(0, ("SCardListReaders: error, not connected"));
        return SCARD_F_INTERNAL_ERROR;
    }
    msg = (char *) malloc(8192);
    pthread_mutex_lock(&g_mutex);
    SET_UINT32(msg, 0, hContext);
    if (send_message(SCARD_LIST_READERS, msg, 4) != 0)
    {
        LLOGLN(0, ("SCardListReaders: error, send_message"));
        free(msg);
        pthread_mutex_unlock(&g_mutex);
        return SCARD_F_INTERNAL_ERROR;
    }
    bytes = 8192;
    if (get_message(&code, msg, &bytes) != 0)
    {
        LLOGLN(0, ("SCardListReaders: error, get_message"));
        free(msg);
        pthread_mutex_unlock(&g_mutex);
        return SCARD_F_INTERNAL_ERROR;
    }
    if (code != SCARD_LIST_READERS)
    {
        LLOGLN(0, ("SCardListReaders: error, bad code"));
        free(msg);
        pthread_mutex_unlock(&g_mutex);
        return SCARD_F_INTERNAL_ERROR;
    }
    pthread_mutex_unlock(&g_mutex);
    offset = 0;
    num_readers = GET_UINT32(msg, offset);
    offset += 4;
    LLOGLN(10, ("hi - mszReaders %p pcchReaders %p num_readers %d", mszReaders, pcchReaders, num_readers));
    reader_names = (char *) malloc(8192);
    reader_names_index = 0;
    for (index = 0; index < num_readers; index++)
    {
        memcpy(reader, msg + offset, 100);
        bytes = strlen(reader);
        memcpy(reader_names + reader_names_index, reader, bytes);
        reader_names_index += bytes;
        reader_names[reader_names_index] = 0;
        reader_names_index++;
        offset += 100;
        LLOGLN(10, ("SCardListReaders: readername %s", reader));
    }
    reader_names[reader_names_index] = 0;
    reader_names_index++;
    status = GET_UINT32(msg, offset);
    offset += 4;
    if (pcchReaders != 0)
    {
        *pcchReaders = reader_names_index;
    }
    if (mszReaders != 0)
    {
        memcpy(mszReaders, reader_names, reader_names_index);
    }
    free(msg);
    free(reader_names);
    return status;
}

/*****************************************************************************/
PCSC_API LONG
SCardFreeMemory(SCARDCONTEXT hContext, LPCVOID pvMem)
{
    LLOGLN(0, ("SCardFreeMemory:"));
    if (g_sck == -1)
    {
        LLOGLN(0, ("SCardFreeMemory: error, not connected"));
        return SCARD_F_INTERNAL_ERROR;
    }
    pthread_mutex_lock(&g_mutex);
    pthread_mutex_unlock(&g_mutex);
    return SCARD_S_SUCCESS;
}

/*****************************************************************************/
PCSC_API LONG
SCardCancel(SCARDCONTEXT hContext)
{
    LLOGLN(0, ("SCardCancel:"));
    if (g_sck == -1)
    {
        LLOGLN(0, ("SCardCancel: error, not connected"));
        return SCARD_F_INTERNAL_ERROR;
    }
    pthread_mutex_lock(&g_mutex);
    pthread_mutex_unlock(&g_mutex);
    return SCARD_S_SUCCESS;
}

/*****************************************************************************/
PCSC_API LONG
SCardGetAttrib(SCARDHANDLE hCard, DWORD dwAttrId, LPBYTE pbAttr,
               LPDWORD pcbAttrLen)
{
    LLOGLN(0, ("SCardGetAttrib:"));
    if (g_sck == -1)
    {
        LLOGLN(0, ("SCardGetAttrib: error, not connected"));
        return SCARD_F_INTERNAL_ERROR;
    }
    pthread_mutex_lock(&g_mutex);
    pthread_mutex_unlock(&g_mutex);
    return SCARD_S_SUCCESS;
}

/*****************************************************************************/
PCSC_API LONG
SCardSetAttrib(SCARDHANDLE hCard, DWORD dwAttrId, LPCBYTE pbAttr,
               DWORD cbAttrLen)
{
    LLOGLN(0, ("SCardSetAttrib:"));
    if (g_sck == -1)
    {
        LLOGLN(0, ("SCardSetAttrib: error, not connected"));
        return SCARD_F_INTERNAL_ERROR;
    }
    pthread_mutex_lock(&g_mutex);
    pthread_mutex_unlock(&g_mutex);
    return SCARD_S_SUCCESS;
}

/*****************************************************************************/
char *
pcsc_stringify_error(const long code)
{
    switch (code)
    {
        case SCARD_S_SUCCESS:
            snprintf(g_error_str, 511, "Command successful.");
            break;
        case SCARD_F_INTERNAL_ERROR:
            snprintf(g_error_str, 511, "Internal error.");
            break;
        default:
            snprintf(g_error_str, 511, "error 0x%8.8x", (int)code);
            break;
    }
    g_error_str[511] = 0;
    return g_error_str;
}
