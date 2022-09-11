
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <windows.h>

#include "winscard-funcs.h"

#define LUNUSED(_param) (void) _param

static tSCardEstablishContext aSCardEstablishContext;
static tSCardReleaseContext aSCardReleaseContext;
static tSCardIsValidContext aSCardIsValidContext;
static tSCardListReaderGroupsA aSCardListReaderGroupsA;
static tSCardListReaderGroupsW aSCardListReaderGroupsW;
static tSCardListReadersA aSCardListReadersA;
static tSCardListReadersW aSCardListReadersW;
static tSCardListCardsA aSCardListCardsA;
static tSCardListCardsW aSCardListCardsW;
static tSCardListInterfacesA aSCardListInterfacesA;
static tSCardListInterfacesW aSCardListInterfacesW;
static tSCardGetProviderIdA aSCardGetProviderIdA;
static tSCardGetProviderIdW aSCardGetProviderIdW;
static tSCardGetCardTypeProviderNameA aSCardGetCardTypeProviderNameA;
static tSCardGetCardTypeProviderNameW aSCardGetCardTypeProviderNameW;
static tSCardIntroduceReaderGroupA aSCardIntroduceReaderGroupA;
static tSCardIntroduceReaderGroupW aSCardIntroduceReaderGroupW;
static tSCardForgetReaderGroupA aSCardForgetReaderGroupA;
static tSCardForgetReaderGroupW aSCardForgetReaderGroupW;
static tSCardIntroduceReaderA aSCardIntroduceReaderA;
static tSCardIntroduceReaderW aSCardIntroduceReaderW;
static tSCardForgetReaderA aSCardForgetReaderA;
static tSCardForgetReaderW aSCardForgetReaderW;
static tSCardAddReaderToGroupA aSCardAddReaderToGroupA;
static tSCardAddReaderToGroupW aSCardAddReaderToGroupW;
static tSCardRemoveReaderFromGroupA aSCardRemoveReaderFromGroupA;
static tSCardRemoveReaderFromGroupW aSCardRemoveReaderFromGroupW;
static tSCardIntroduceCardTypeA aSCardIntroduceCardTypeA;
static tSCardIntroduceCardTypeW aSCardIntroduceCardTypeW;
static tSCardSetCardTypeProviderNameA aSCardSetCardTypeProviderNameA;
static tSCardSetCardTypeProviderNameW aSCardSetCardTypeProviderNameW;
static tSCardForgetCardTypeA aSCardForgetCardTypeA;
static tSCardForgetCardTypeW aSCardForgetCardTypeW;
static tSCardFreeMemory aSCardFreeMemory;
static tSCardLocateCardsA aSCardLocateCardsA;
static tSCardLocateCardsW aSCardLocateCardsW;
static tSCardGetStatusChangeA aSCardGetStatusChangeA;
static tSCardGetStatusChangeW aSCardGetStatusChangeW;
static tSCardCancel aSCardCancel;
static tSCardConnectA aSCardConnectA;
static tSCardConnectW aSCardConnectW;
static tSCardReconnect aSCardReconnect;
static tSCardDisconnect aSCardDisconnect;
static tSCardBeginTransaction aSCardBeginTransaction;
static tSCardEndTransaction aSCardEndTransaction;
static tSCardCancelTransaction aSCardCancelTransaction;
static tSCardState aSCardState;
static tSCardStatusA aSCardStatusA;
static tSCardStatusW aSCardStatusW;
static tSCardTransmit aSCardTransmit;
static tSCardControl aSCardControl;
static tSCardGetAttrib aSCardGetAttrib;
static tSCardSetAttrib aSCardSetAttrib;

//__declspec(dllexport) const SCARD_IO_REQUEST g_rgSCardT0Pci = { 0 };
//__declspec(dllexport) const SCARD_IO_REQUEST g_rgSCardT1Pci = { 0 };
//__declspec(dllexport) const SCARD_IO_REQUEST g_rgSCardRawPci = { 0 };

static int g_true = 1;

#define LLOGLN(_level, _args) do { if ((_level < 11) && g_true) { writeln _args ; } } while (0)

/*****************************************************************************/
static int
writeln(const char *format, ...)
{
    va_list ap;
    char text[256];

    va_start(ap, format);
    vsnprintf(text, 255, format, ap);
    va_end(ap);
    OutputDebugString(text);
    return 0;
}

#define LLOAD(_func, _type, _name) \
    do { \
        _func = (_type) GetProcAddress(lib, _name); \
        if (_func == 0) \
        { \
            writeln("LLOAD error %s", _name); \
        } \
    } while (0)

static int g_funcs_loaded = 0;

/*****************************************************************************/
static int __fastcall
load_funcs(void)
{
    HMODULE lib;

    if (g_funcs_loaded)
    {
        return 0;
    }
    g_funcs_loaded = 1;
    lib = LoadLibrary("winscard-org.dll");
    LLOGLN(0, ("load_funcs: lib %p", lib));
    LLOAD(aSCardEstablishContext, tSCardEstablishContext, "SCardEstablishContext");
    LLOAD(aSCardReleaseContext, tSCardReleaseContext, "SCardReleaseContext");
    LLOAD(aSCardIsValidContext, tSCardIsValidContext, "SCardIsValidContext");
    LLOAD(aSCardListReaderGroupsA, tSCardListReaderGroupsA, "SCardListReaderGroupsA");
    LLOAD(aSCardListReaderGroupsW, tSCardListReaderGroupsW, "SCardListReaderGroupsW");
    LLOAD(aSCardListReadersA, tSCardListReadersA, "SCardListReadersA");
    LLOAD(aSCardListReadersW, tSCardListReadersW, "SCardListReadersW");
    LLOAD(aSCardListCardsA, tSCardListCardsA, "SCardListCardsA");
    LLOAD(aSCardListCardsW, tSCardListCardsW, "SCardListCardsW");
    LLOAD(aSCardListInterfacesA, tSCardListInterfacesA, "SCardListInterfacesA");
    LLOAD(aSCardListInterfacesW, tSCardListInterfacesW, "SCardListInterfacesW");
    LLOAD(aSCardGetProviderIdA, tSCardGetProviderIdA, "SCardGetProviderIdA");
    LLOAD(aSCardGetProviderIdW, tSCardGetProviderIdW, "SCardGetProviderIdW");
    LLOAD(aSCardGetCardTypeProviderNameA, tSCardGetCardTypeProviderNameA, "SCardGetCardTypeProviderNameA");
    LLOAD(aSCardGetCardTypeProviderNameW, tSCardGetCardTypeProviderNameW, "SCardGetCardTypeProviderNameW");
    LLOAD(aSCardIntroduceReaderGroupA, tSCardIntroduceReaderGroupA, "SCardIntroduceReaderGroupA");
    LLOAD(aSCardIntroduceReaderGroupW, tSCardIntroduceReaderGroupW, "SCardIntroduceReaderGroupW");
    LLOAD(aSCardForgetReaderGroupA, tSCardForgetReaderGroupA, "SCardForgetReaderGroupA");
    LLOAD(aSCardForgetReaderGroupW, tSCardForgetReaderGroupW, "SCardForgetReaderGroupW");
    LLOAD(aSCardIntroduceReaderA, tSCardIntroduceReaderA, "SCardIntroduceReaderA");
    LLOAD(aSCardIntroduceReaderW, tSCardIntroduceReaderW, "SCardIntroduceReaderW");
    LLOAD(aSCardForgetReaderA, tSCardForgetReaderA, "SCardForgetReaderA");
    LLOAD(aSCardForgetReaderW, tSCardForgetReaderW, "SCardForgetReaderW");
    LLOAD(aSCardAddReaderToGroupA, tSCardAddReaderToGroupA, "SCardAddReaderToGroupA");
    LLOAD(aSCardAddReaderToGroupW, tSCardAddReaderToGroupW, "SCardAddReaderToGroupW");
    LLOAD(aSCardRemoveReaderFromGroupA, tSCardRemoveReaderFromGroupA, "SCardRemoveReaderFromGroupA");
    LLOAD(aSCardRemoveReaderFromGroupW, tSCardRemoveReaderFromGroupW, "SCardRemoveReaderFromGroupW");
    LLOAD(aSCardIntroduceCardTypeA, tSCardIntroduceCardTypeA, "SCardIntroduceCardTypeA");
    LLOAD(aSCardIntroduceCardTypeW, tSCardIntroduceCardTypeW, "SCardIntroduceCardTypeW");
    LLOAD(aSCardSetCardTypeProviderNameA, tSCardSetCardTypeProviderNameA, "SCardSetCardTypeProviderNameA");
    LLOAD(aSCardSetCardTypeProviderNameW, tSCardSetCardTypeProviderNameW, "SCardSetCardTypeProviderNameW");
    LLOAD(aSCardForgetCardTypeA, tSCardForgetCardTypeA, "SCardForgetCardTypeA");
    LLOAD(aSCardForgetCardTypeW, tSCardForgetCardTypeW, "SCardForgetCardTypeW");
    LLOAD(aSCardFreeMemory, tSCardFreeMemory, "SCardFreeMemory");
    LLOAD(aSCardLocateCardsA, tSCardLocateCardsA, "SCardLocateCardsA");
    LLOAD(aSCardLocateCardsW, tSCardLocateCardsW, "SCardLocateCardsW");
    LLOAD(aSCardGetStatusChangeA, tSCardGetStatusChangeA, "SCardGetStatusChangeA");
    LLOAD(aSCardGetStatusChangeW, tSCardGetStatusChangeW, "SCardGetStatusChangeW");
    LLOAD(aSCardCancel, tSCardCancel, "SCardCancel");
    LLOAD(aSCardConnectA, tSCardConnectA, "SCardConnectA");
    LLOAD(aSCardConnectW, tSCardConnectW, "SCardConnectW");
    LLOAD(aSCardReconnect, tSCardReconnect, "SCardReconnect");
    LLOAD(aSCardDisconnect, tSCardDisconnect, "SCardDisconnect");
    LLOAD(aSCardBeginTransaction, tSCardBeginTransaction, "SCardBeginTransaction");
    LLOAD(aSCardEndTransaction, tSCardEndTransaction, "SCardEndTransaction");
    LLOAD(aSCardCancelTransaction, tSCardCancelTransaction, "SCardCancelTransaction");
    LLOAD(aSCardState, tSCardState, "SCardState");
    LLOAD(aSCardStatusA, tSCardStatusA, "SCardStatusA");
    LLOAD(aSCardStatusW, tSCardStatusW, "SCardStatusW");
    LLOAD(aSCardTransmit, tSCardTransmit, "SCardTransmit");
    LLOAD(aSCardControl, tSCardControl, "SCardControl");
    LLOAD(aSCardGetAttrib, tSCardGetAttrib, "SCardGetAttrib");
    LLOAD(aSCardSetAttrib, tSCardSetAttrib, "SCardSetAttrib");

    return 0;
}

/*****************************************************************************/
BOOL WINAPI
DllEntryPoint(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    BOOL rv;
    LUNUSED(hinstDLL);
    LUNUSED(lpvReserved);

    LLOGLN(10, ("DllEntryPoint: hinstDLL %p fdwReason %d", hinstDLL, fdwReason));
    rv = FALSE;
    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            LLOGLN(0, ("DllEntryPoint: DLL_PROCESS_ATTACH"));
            load_funcs();
            rv = TRUE;
            break;
        case DLL_THREAD_ATTACH:
            LLOGLN(10, ("DllEntryPoint: DLL_THREAD_ATTACH"));
            rv = TRUE;
            break;
        case DLL_THREAD_DETACH:
            LLOGLN(10, ("DllEntryPoint: DLL_THREAD_DETACH"));
            rv = TRUE;
            break;
        case DLL_PROCESS_DETACH:
            LLOGLN(0, ("DllEntryPoint: DLL_PROCESS_DETACH"));
            rv = TRUE;
            break;
        default:
            LLOGLN(0, ("DllEntryPoint: unknown fdwReason %d", fdwReason));
            break;

    }
    return rv;
}

/*****************************************************************************/
LONG WINAPI
SCardEstablishContext(DWORD dwScope, LPCVOID pvReserved1,
                      LPCVOID pvReserved2, LPSCARDCONTEXT phContext)
{
    LLOGLN(0, ("SCardEstablishContext:"));
    return aSCardEstablishContext(dwScope, pvReserved1, pvReserved2, phContext);
}

/*****************************************************************************/
LONG WINAPI
SCardReleaseContext(SCARDCONTEXT hContext)
{
    LLOGLN(0, ("SCardReleaseContext:"));
    return aSCardReleaseContext(hContext);
}

/*****************************************************************************/
LONG WINAPI
SCardIsValidContext(SCARDCONTEXT hContext)
{
    LLOGLN(0, ("SCardIsValidContext:"));
    return aSCardIsValidContext(hContext);
}

/*****************************************************************************/
LONG WINAPI
SCardListReaderGroupsA(SCARDCONTEXT hContext, LPSTR mszGroups,
                       LPDWORD pcchGroups)
{
    LLOGLN(0, ("SCardListReaderGroupsA:"));
    return aSCardListReaderGroupsA(hContext, mszGroups, pcchGroups);
}

/*****************************************************************************/
LONG WINAPI
SCardListReaderGroupsW(SCARDCONTEXT hContext, LPWSTR mszGroups,
                       LPDWORD pcchGroups)
{
    LLOGLN(0, ("SCardListReaderGroupsW:"));
    return aSCardListReaderGroupsW(hContext, mszGroups, pcchGroups);
}

/*****************************************************************************/
LONG WINAPI
SCardListReadersA(SCARDCONTEXT hContext, LPCSTR mszGroups,
                  LPSTR mszReaders, LPDWORD pcchReaders)
{
    LLOGLN(0, ("SCardListReadersA:"));
    return aSCardListReadersA(hContext, mszGroups, mszReaders, pcchReaders);
}

/*****************************************************************************/
LONG WINAPI
SCardListReadersW(SCARDCONTEXT hContext, LPCWSTR mszGroups,
                  LPWSTR mszReaders, LPDWORD pcchReaders)
{
    char text[256];
    LONG rv;
    DWORD cchReaders;

    text[0] = 0;
    if (mszGroups != 0)
    {
        wcstombs(text, mszGroups, 255);
    }
    cchReaders = *pcchReaders;
    LLOGLN(0, ("SCardListReadersW: mszGroups [%s] cchReaders [%d]", text, *pcchReaders));
    rv = aSCardListReadersW(hContext, mszGroups, mszReaders, pcchReaders);
    text[0] = 0;
    if (cchReaders > 0)
    {
        wcstombs(text, mszReaders, 255);
    }
    LLOGLN(0, ("  mszReaders [%s] cchReaders [%d]", text, *pcchReaders));
    return rv;
}

/*****************************************************************************/
LONG WINAPI
SCardListCardsA(SCARDCONTEXT hContext, LPCBYTE pbAtr,
                LPCGUID rgquidInterfaces, DWORD cguidInterfaceCount,
                LPSTR mszCards, LPDWORD pcchCards)
{
    LLOGLN(0, ("SCardListCardsA:"));
    return aSCardListCardsA(hContext, pbAtr, rgquidInterfaces,
                            cguidInterfaceCount, mszCards, pcchCards);
}

/*****************************************************************************/
LONG WINAPI
SCardListCardsW(SCARDCONTEXT hContext, LPCBYTE pbAtr,
                LPCGUID rgquidInterfaces, DWORD cguidInterfaceCount,
                LPWSTR mszCards, LPDWORD pcchCards)
{
    LLOGLN(0, ("SCardListCardsW:"));
    return aSCardListCardsW(hContext, pbAtr, rgquidInterfaces,
                            cguidInterfaceCount, mszCards, pcchCards);
}

/*****************************************************************************/
LONG WINAPI
SCardListInterfacesA(SCARDCONTEXT hContext, LPCSTR szCard,
                     LPGUID pguidInterfaces, LPDWORD pcguidInterfaces)
{
    LLOGLN(0, ("SCardListInterfacesA:"));
    return aSCardListInterfacesA(hContext, szCard, pguidInterfaces,
                                 pcguidInterfaces);
}

/*****************************************************************************/
LONG WINAPI
SCardListInterfacesW(SCARDCONTEXT hContext, LPCWSTR szCard,
                     LPGUID pguidInterfaces, LPDWORD pcguidInterfaces)
{
    LLOGLN(0, ("SCardListInterfacesW:"));
    return aSCardListInterfacesW(hContext, szCard, pguidInterfaces,
                                 pcguidInterfaces);
}

/*****************************************************************************/
LONG WINAPI
SCardGetProviderIdA(SCARDCONTEXT hContext, LPCSTR szCard,
                    LPGUID pguidProviderId)
{
    LLOGLN(0, ("SCardGetProviderIdA:"));
    return aSCardGetProviderIdA(hContext, szCard, pguidProviderId);
}

/****************************************************************************/
LONG WINAPI
SCardGetProviderIdW(SCARDCONTEXT hContext, LPCWSTR szCard,
                    LPGUID pguidProviderId)
{
    LLOGLN(0, ("SCardGetProviderIdW:"));
    return aSCardGetProviderIdW(hContext, szCard, pguidProviderId);
}

/****************************************************************************/
LONG WINAPI
SCardGetCardTypeProviderNameA(SCARDCONTEXT hContext, LPCSTR szCardName,
                              DWORD dwProviderId, LPSTR szProvider,
                              LPDWORD pcchProvider)
{
    LLOGLN(0, ("SCardGetCardTypeProviderNameA:"));
    return aSCardGetCardTypeProviderNameA(hContext, szCardName, dwProviderId,
                                          szProvider, pcchProvider);
}

/*****************************************************************************/
LONG WINAPI
SCardGetCardTypeProviderNameW(SCARDCONTEXT hContext, LPCWSTR szCardName,
                              DWORD dwProviderId, LPWSTR szProvider,
                              LPDWORD pcchProvider)
{
    LLOGLN(0, ("SCardGetCardTypeProviderNameW:"));
    return aSCardGetCardTypeProviderNameW(hContext, szCardName, dwProviderId,
                                          szProvider, pcchProvider);
}

/*****************************************************************************/
LONG WINAPI
SCardIntroduceReaderGroupA(SCARDCONTEXT hContext, LPCSTR szGroupName)
{
    LLOGLN(0, ("SCardIntroduceReaderGroupA:"));
    return aSCardIntroduceReaderGroupA(hContext, szGroupName);
}

/*****************************************************************************/
LONG WINAPI
SCardIntroduceReaderGroupW(SCARDCONTEXT hContext, LPCWSTR szGroupName)
{
    LLOGLN(0, ("SCardIntroduceReaderGroupW:"));
    return aSCardIntroduceReaderGroupW(hContext, szGroupName);
}

/*****************************************************************************/
LONG WINAPI
SCardForgetReaderGroupA(SCARDCONTEXT hContext, LPCSTR szGroupName)
{
    LLOGLN(0, ("SCardForgetReaderGroupA:"));
    return aSCardForgetReaderGroupA(hContext, szGroupName);
}

/*****************************************************************************/
LONG WINAPI
SCardForgetReaderGroupW(SCARDCONTEXT hContext, LPCWSTR szGroupName)
{
    LLOGLN(0, ("SCardForgetReaderGroupW:"));
    return aSCardForgetReaderGroupW(hContext, szGroupName);
}

/*****************************************************************************/
LONG WINAPI
SCardIntroduceReaderA(SCARDCONTEXT hContext, LPCSTR szReaderName,
                      LPCSTR szDeviceName)
{
    LLOGLN(0, ("SCardIntroduceReaderA:"));
    return aSCardIntroduceReaderA(hContext, szReaderName, szDeviceName);
}

/*****************************************************************************/
LONG WINAPI
SCardIntroduceReaderW(SCARDCONTEXT hContext, LPCWSTR szReaderName,
                      LPCWSTR szDeviceName)
{
    LLOGLN(0, ("SCardIntroduceReaderW:"));
    return aSCardIntroduceReaderW(hContext, szReaderName, szDeviceName);
}

/*****************************************************************************/
LONG WINAPI
SCardForgetReaderA(SCARDCONTEXT hContext, LPCSTR szReaderName)
{
    LLOGLN(0, ("SCardForgetReaderA:"));
    return aSCardForgetReaderA(hContext, szReaderName);
}

/*****************************************************************************/
LONG WINAPI
SCardForgetReaderW(SCARDCONTEXT hContext, LPCWSTR szReaderName)
{
    LLOGLN(0, ("SCardForgetReaderW:"));
    return aSCardForgetReaderW(hContext, szReaderName);
}

/*****************************************************************************/
LONG WINAPI
SCardAddReaderToGroupA(SCARDCONTEXT hContext, LPCSTR szReaderName,
                       LPCSTR szGroupName)
{
    LLOGLN(0, ("SCardAddReaderToGroupA:"));
    return aSCardAddReaderToGroupA(hContext, szReaderName, szGroupName);
}

/*****************************************************************************/
LONG WINAPI
SCardAddReaderToGroupW(SCARDCONTEXT hContext, LPCWSTR szReaderName,
                       LPCWSTR szGroupName)
{
    LLOGLN(0, ("SCardAddReaderToGroupW:"));
    return aSCardAddReaderToGroupW(hContext, szReaderName, szGroupName);
}

/*****************************************************************************/
LONG WINAPI
SCardRemoveReaderFromGroupA(SCARDCONTEXT hContext, LPCSTR szReaderName,
                            LPCSTR szGroupName)
{
    LLOGLN(0, ("SCardRemoveReaderFromGroupA:"));
    return aSCardRemoveReaderFromGroupA(hContext, szReaderName, szGroupName);
}

/*****************************************************************************/
LONG WINAPI
SCardRemoveReaderFromGroupW(SCARDCONTEXT hContext, LPCWSTR szReaderName,
                            LPCWSTR szGroupName)
{
    LLOGLN(0, ("SCardRemoveReaderFromGroupW:"));
    return aSCardRemoveReaderFromGroupW(hContext, szReaderName, szGroupName);
}

/*****************************************************************************/
LONG WINAPI
SCardIntroduceCardTypeA(SCARDCONTEXT hContext, LPCSTR szCardName,
                        LPCGUID pguidPrimaryProvider,
                        LPCGUID rgguidInterfaces,
                        DWORD dwInterfaceCount, LPCBYTE pbAtr,
                        LPCBYTE pbAtrMask, DWORD cbAtrLen)
{
    LLOGLN(0, ("SCardIntroduceCardTypeA:"));
    return aSCardIntroduceCardTypeA(hContext, szCardName, pguidPrimaryProvider,
                                    rgguidInterfaces, dwInterfaceCount, pbAtr,
                                    pbAtrMask, cbAtrLen);
}

/*****************************************************************************/
LONG WINAPI
SCardIntroduceCardTypeW(SCARDCONTEXT hContext, LPCWSTR szCardName,
                        LPCGUID pguidPrimaryProvider, LPCGUID rgguidInterfaces,
                        DWORD dwInterfaceCount, LPCBYTE pbAtr,
                        LPCBYTE pbAtrMask, DWORD cbAtrLen)
{
    LLOGLN(0, ("SCardIntroduceCardTypeW:"));
    return aSCardIntroduceCardTypeW(hContext, szCardName, pguidPrimaryProvider,
                                    rgguidInterfaces, dwInterfaceCount, pbAtr,
                                    pbAtrMask, cbAtrLen);
}

/*****************************************************************************/
LONG WINAPI
SCardSetCardTypeProviderNameA(SCARDCONTEXT hContext, LPCSTR szCardName,
                              DWORD dwProviderId, LPCSTR szProvider)
{
    LLOGLN(0, ("SCardSetCardTypeProviderNameA:"));
    return aSCardSetCardTypeProviderNameA(hContext, szCardName,
                                          dwProviderId, szProvider);
}

/*****************************************************************************/
LONG WINAPI
SCardSetCardTypeProviderNameW(SCARDCONTEXT hContext, LPCWSTR szCardName,
                              DWORD dwProviderId, LPCWSTR szProvider)
{
    LLOGLN(0, ("SCardSetCardTypeProviderNameW:"));
    return aSCardSetCardTypeProviderNameW(hContext, szCardName,
                                          dwProviderId, szProvider);
}

/*****************************************************************************/
LONG WINAPI
SCardForgetCardTypeA(SCARDCONTEXT hContext, LPCSTR szCardName)
{
    LLOGLN(0, ("SCardForgetCardTypeA:"));
    return aSCardForgetCardTypeA(hContext, szCardName);
}

/*****************************************************************************/
LONG WINAPI
SCardForgetCardTypeW(SCARDCONTEXT hContext, LPCWSTR szCardName)
{
    LLOGLN(0, ("SCardForgetCardTypeW:"));
    return aSCardForgetCardTypeW(hContext, szCardName);
}

/*****************************************************************************/
LONG WINAPI
SCardFreeMemory(SCARDCONTEXT hContext, LPCVOID pvMem)
{
    LLOGLN(0, ("SCardFreeMemory:"));
    return aSCardFreeMemory(hContext, pvMem);
}

/*****************************************************************************/
LONG WINAPI
SCardLocateCardsA(SCARDCONTEXT hContext, LPCSTR mszCards,
                  LPSCARD_READERSTATEA rgReaderStates, DWORD cReaders)
{
    LLOGLN(0, ("SCardLocateCardsA:"));
    return aSCardLocateCardsA(hContext, mszCards, rgReaderStates, cReaders);
}

/*****************************************************************************/
LONG WINAPI
SCardLocateCardsW(SCARDCONTEXT hContext, LPCWSTR mszCards,
                  LPSCARD_READERSTATEW rgReaderStates, DWORD cReaders)
{
    LLOGLN(0, ("SCardLocateCardsW:"));
    return aSCardLocateCardsW(hContext, mszCards, rgReaderStates, cReaders);
}

/*****************************************************************************/
LONG WINAPI
SCardGetStatusChangeA(SCARDCONTEXT hContext, DWORD dwTimeout,
                      LPSCARD_READERSTATEA rgReaderStates,
                      DWORD cReaders)
{
    LLOGLN(0, ("SCardGetStatusChangeA:"));
    return aSCardGetStatusChangeA(hContext, dwTimeout, rgReaderStates, cReaders);
}

/*****************************************************************************/
LONG WINAPI
SCardGetStatusChangeW(SCARDCONTEXT hContext, DWORD dwTimeout,
                      LPSCARD_READERSTATEW rgReaderStates,
                      DWORD cReaders)
{
    LLOGLN(0, ("SCardGetStatusChangeW:"));
    return aSCardGetStatusChangeW(hContext, dwTimeout, rgReaderStates, cReaders);
}

/*****************************************************************************/
LONG WINAPI
SCardCancel(SCARDCONTEXT hContext)
{
    LLOGLN(0, ("SCardCancel:"));
    return aSCardCancel(hContext);
}

/*****************************************************************************/
LONG WINAPI
SCardConnectA(SCARDCONTEXT hContext, LPCSTR szReader, DWORD dwShareMode,
              DWORD dwPreferredProtocols, LPSCARDHANDLE phCard,
              LPDWORD pdwActiveProtocol)
{
    LLOGLN(0, ("SCardConnectA:"));
    return aSCardConnectA(hContext, szReader, dwShareMode,
                          dwPreferredProtocols, phCard,
                          pdwActiveProtocol);
}

/*****************************************************************************/
LONG WINAPI
SCardConnectW(SCARDCONTEXT hContext, LPCWSTR szReader, DWORD dwShareMode,
              DWORD dwPreferredProtocols, LPSCARDHANDLE phCard,
              LPDWORD pdwActiveProtocol)
{
    LLOGLN(0, ("SCardConnectW:"));
    return aSCardConnectW(hContext, szReader, dwShareMode,
                          dwPreferredProtocols, phCard,
                          pdwActiveProtocol);
}

/*****************************************************************************/
LONG WINAPI
SCardReconnect(SCARDHANDLE hCard, DWORD dwShareMode,
               DWORD dwPreferredProtocols, DWORD dwInitialization,
               LPDWORD pdwActiveProtocol)
{
    LLOGLN(0, ("SCardReconnect:"));
    return SCardReconnect(hCard, dwShareMode, dwPreferredProtocols,
                          dwInitialization, pdwActiveProtocol);
}

/*****************************************************************************/
LONG WINAPI
SCardDisconnect(SCARDHANDLE hCard, DWORD dwDisposition)
{
    LLOGLN(0, ("SCardDisconnect:"));
    return aSCardDisconnect(hCard, dwDisposition);
}

/*****************************************************************************/
LONG WINAPI
SCardBeginTransaction(SCARDHANDLE hCard)
{
    LLOGLN(0, ("SCardBeginTransaction:"));
    return aSCardBeginTransaction(hCard);
}

/*****************************************************************************/
LONG WINAPI
SCardEndTransaction(SCARDHANDLE hCard, DWORD dwDisposition)
{
    LLOGLN(0, ("SCardEndTransaction:"));
    return aSCardEndTransaction(hCard, dwDisposition);
}

/*****************************************************************************/
LONG WINAPI
SCardCancelTransaction(SCARDHANDLE hCard)
{
    LLOGLN(0, ("SCardCancelTransaction:"));
    return aSCardCancelTransaction(hCard);
}

/*****************************************************************************/
LONG WINAPI
SCardState(SCARDHANDLE hCard, LPDWORD pdwState, LPDWORD pdwProtocol,
           LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
    LLOGLN(0, ("SCardState:"));
    return aSCardState(hCard, pdwState, pdwProtocol, pbAtr, pcbAtrLen);
}

/*****************************************************************************/
LONG WINAPI
SCardStatusA(SCARDHANDLE hCard, LPSTR szReaderName, LPDWORD pcchReaderLen,
             LPDWORD pdwState, LPDWORD pdwProtocol, LPBYTE pbAtr,
             LPDWORD pcbAtrLen)
{
    LLOGLN(0, ("SCardStatusA:"));
    return aSCardStatusA(hCard, szReaderName, pcchReaderLen,
                         pdwState, pdwProtocol, pbAtr, pcbAtrLen);
}

/*****************************************************************************/
LONG WINAPI
SCardStatusW(SCARDHANDLE hCard, LPWSTR szReaderName, LPDWORD pcchReaderLen,
             LPDWORD pdwState, LPDWORD pdwProtocol, LPBYTE pbAtr,
             LPDWORD pcbAtrLen)
{
    LONG rv;

    LLOGLN(0, ("SCardStatusW:"));
    LLOGLN(0, ("  cchReaderLen %d", *pcchReaderLen));
    LLOGLN(0, ("  cbAtrLen %d", *pcbAtrLen));
    rv = aSCardStatusW(hCard, szReaderName, pcchReaderLen,
                       pdwState, pdwProtocol, pbAtr, pcbAtrLen);
    LLOGLN(0, ("  rv %d cchReaderLen %d", rv, *pcchReaderLen));
    return rv;
}

/*****************************************************************************/
LONG WINAPI
SCardTransmit(SCARDHANDLE hCard, LPCSCARD_IO_REQUEST pioSendPci,
              LPCBYTE pbSendBuffer, DWORD cbSendLength,
              LPSCARD_IO_REQUEST pioRecvPci, LPBYTE pbRecvBuffer,
              LPDWORD pcbRecvLength)
{
    LONG rv;

    LLOGLN(10, ("SCardTransmit:"));
    LLOGLN(10, ("  hCard %p", hCard));
    LLOGLN(10, ("  cbSendLength %d", cbSendLength));
    LLOGLN(10, ("  cbRecvLength %d", *pcbRecvLength));
    LLOGLN(10, ("  pioSendPci->dwProtocol %d", pioSendPci->dwProtocol));
    LLOGLN(10, ("  pioSendPci->cbPciLength %d", pioSendPci->cbPciLength));
    LLOGLN(10, ("  pioRecvPci %p", pioRecvPci));
    if (pioRecvPci != NULL)
    {
        LLOGLN(10, ("    pioRecvPci->dwProtocol %d", pioRecvPci->dwProtocol));
        LLOGLN(10, ("    pioRecvPci->cbPciLength %d", pioRecvPci->cbPciLength));
    }
    rv = aSCardTransmit(hCard, pioSendPci, pbSendBuffer, cbSendLength,
                        pioRecvPci, pbRecvBuffer, pcbRecvLength);
    LLOGLN(10, ("  rv %d cbRecvLength %d", rv, *pcbRecvLength));
    return rv;
}

/*****************************************************************************/
LONG WINAPI
SCardControl(SCARDHANDLE hCard, DWORD dwControlCode, LPCVOID lpInBuffer,
             DWORD nInBufferSize, LPVOID lpOutBuffer,
             DWORD nOutBufferSize, LPDWORD lpBytesReturned)
{
    LONG rv;
    char text[8148];

    LLOGLN(10, ("SCardControl:"));
    LLOGLN(10, ("  hCard %p", hCard));
    LLOGLN(10, ("  dwControlCode 0x%8.8x", dwControlCode));
    LLOGLN(10, ("  lpInBuffer %p", lpInBuffer));
    LLOGLN(10, ("  nInBufferSize %d", nInBufferSize));
    LLOGLN(10, ("  lpOutBuffer %p", lpOutBuffer));
    LLOGLN(10, ("  nOutBufferSize %d", nOutBufferSize));
    LLOGLN(10, ("  lpBytesReturned %p", lpBytesReturned));
    rv = aSCardControl(hCard, dwControlCode,
                       lpInBuffer, nInBufferSize,
                       lpOutBuffer, nOutBufferSize,
                       lpBytesReturned);
    LLOGLN(10, ("  rv %d BytesReturned %d", rv, *lpBytesReturned));
    return rv;
}

/*****************************************************************************/
LONG WINAPI
SCardGetAttrib(SCARDHANDLE hCard, DWORD dwAttrId, LPBYTE pbAttr,
               LPDWORD pcbAttrLen)
{
    LLOGLN(0, ("SCardGetAttrib:"));
    return aSCardGetAttrib(hCard, dwAttrId, pbAttr, pcbAttrLen);
}

/*****************************************************************************/
LONG WINAPI
SCardSetAttrib(SCARDHANDLE hCard, DWORD dwAttrId, LPCBYTE pbAttr,
               DWORD cbAttrLen)
{
    LLOGLN(0, ("SCardSetAttrib:"));
    return aSCardSetAttrib(hCard, dwAttrId, pbAttr, cbAttrLen);
}
