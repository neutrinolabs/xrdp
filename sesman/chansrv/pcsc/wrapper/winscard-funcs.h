
#ifndef _WINSCARD_FUNCS_H
#define _WINSCARD_FUNCS_H

typedef LONG WINAPI
(*tSCardEstablishContext)(DWORD dwScope, LPCVOID pvReserved1,
                          LPCVOID pvReserved2, LPSCARDCONTEXT phContext);
typedef LONG WINAPI
(*tSCardReleaseContext)(SCARDCONTEXT hContext);
typedef LONG WINAPI
(*tSCardIsValidContext)(SCARDCONTEXT hContext);
typedef LONG WINAPI
(*tSCardListReaderGroupsA)(SCARDCONTEXT hContext, LPSTR mszGroups,
                           LPDWORD pcchGroups);
typedef LONG WINAPI
(*tSCardListReaderGroupsW)(SCARDCONTEXT hContext, LPWSTR mszGroups,
                           LPDWORD pcchGroups);
typedef LONG WINAPI
(*tSCardListReadersA)(SCARDCONTEXT hContext, LPCSTR mszGroups,
                      LPSTR mszReaders, LPDWORD pcchReaders);
typedef LONG WINAPI
(*tSCardListReadersW)(SCARDCONTEXT hContext, LPCWSTR mszGroups,
                      LPWSTR mszReaders, LPDWORD pcchReaders);
typedef LONG WINAPI
(*tSCardListCardsA)(SCARDCONTEXT hContext, LPCBYTE pbAtr,
                    LPCGUID rgquidInterfaces, DWORD cguidInterfaceCount,
                    LPSTR mszCards, LPDWORD pcchCards);
typedef LONG WINAPI
(*tSCardListCardsW)(SCARDCONTEXT hContext, LPCBYTE pbAtr,
                    LPCGUID rgquidInterfaces, DWORD cguidInterfaceCount,
                    LPWSTR mszCards, LPDWORD pcchCards);
typedef LONG WINAPI
(*tSCardListInterfacesA)(SCARDCONTEXT hContext, LPCSTR szCard,
                         LPGUID pguidInterfaces, LPDWORD pcguidInterfaces);
typedef LONG WINAPI
(*tSCardListInterfacesW)(SCARDCONTEXT hContext, LPCWSTR szCard,
                         LPGUID pguidInterfaces, LPDWORD pcguidInterfaces);
typedef LONG WINAPI
(*tSCardGetProviderIdA)(SCARDCONTEXT hContext, LPCSTR szCard,
                        LPGUID pguidProviderId);
typedef LONG WINAPI
(*tSCardGetProviderIdW)(SCARDCONTEXT hContext, LPCWSTR szCard,
                        LPGUID pguidProviderId);
typedef LONG WINAPI
(*tSCardGetCardTypeProviderNameA)(SCARDCONTEXT hContext, LPCSTR szCardName,
                                  DWORD dwProviderId, LPSTR szProvider,
                                  LPDWORD pcchProvider);
typedef LONG WINAPI
(*tSCardGetCardTypeProviderNameW)(SCARDCONTEXT hContext, LPCWSTR szCardName,
                                  DWORD dwProviderId, LPWSTR szProvider,
                                  LPDWORD pcchProvider);
typedef LONG WINAPI
(*tSCardIntroduceReaderGroupA)(SCARDCONTEXT hContext, LPCSTR szGroupName);
typedef LONG WINAPI
(*tSCardIntroduceReaderGroupW)(SCARDCONTEXT hContext, LPCWSTR szGroupName);

typedef LONG WINAPI
(*tSCardForgetReaderGroupA)(SCARDCONTEXT hContext, LPCSTR szGroupName);
typedef LONG WINAPI
(*tSCardForgetReaderGroupW)(SCARDCONTEXT hContext, LPCWSTR szGroupName);
typedef LONG WINAPI
(*tSCardIntroduceReaderA)(SCARDCONTEXT hContext, LPCSTR szReaderName,
                          LPCSTR szDeviceName);
typedef LONG WINAPI
(*tSCardIntroduceReaderW)(SCARDCONTEXT hContext, LPCWSTR szReaderName,
                          LPCWSTR szDeviceName);
typedef LONG WINAPI
(*tSCardForgetReaderA)(SCARDCONTEXT hContext, LPCSTR szReaderName);
typedef LONG WINAPI
(*tSCardForgetReaderW)(SCARDCONTEXT hContext, LPCWSTR szReaderName);

typedef LONG WINAPI
(*tSCardAddReaderToGroupA)(SCARDCONTEXT hContext, LPCSTR szReaderName,
                           LPCSTR szGroupName);
typedef LONG WINAPI
(*tSCardAddReaderToGroupW)(SCARDCONTEXT hContext, LPCWSTR szReaderName,
                           LPCWSTR szGroupName);
typedef LONG WINAPI
(*tSCardRemoveReaderFromGroupA)(SCARDCONTEXT hContext, LPCSTR szReaderName,
                                LPCSTR szGroupName);
typedef LONG WINAPI
(*tSCardRemoveReaderFromGroupW)(SCARDCONTEXT hContext, LPCWSTR szReaderName,
                                LPCWSTR szGroupName);
typedef LONG WINAPI
(*tSCardIntroduceCardTypeA)(SCARDCONTEXT hContext, LPCSTR szCardName,
                            LPCGUID pguidPrimaryProvider,
                            LPCGUID rgguidInterfaces,
                            DWORD dwInterfaceCount, LPCBYTE pbAtr,
                            LPCBYTE pbAtrMask, DWORD cbAtrLen);
typedef LONG WINAPI
(*tSCardIntroduceCardTypeW)(SCARDCONTEXT hContext, LPCWSTR szCardName,
                            LPCGUID pguidPrimaryProvider,
                            LPCGUID rgguidInterfaces,
                            DWORD dwInterfaceCount, LPCBYTE pbAtr,
                            LPCBYTE pbAtrMask, DWORD cbAtrLen);
typedef LONG WINAPI
(*tSCardSetCardTypeProviderNameA)(SCARDCONTEXT hContext, LPCSTR szCardName,
                                  DWORD dwProviderId, LPCSTR szProvider);
typedef LONG WINAPI
(*tSCardSetCardTypeProviderNameW)(SCARDCONTEXT hContext, LPCWSTR szCardName,
                                  DWORD dwProviderId, LPCWSTR szProvider);
typedef LONG WINAPI
(*tSCardForgetCardTypeA)(SCARDCONTEXT hContext, LPCSTR szCardName);
typedef LONG WINAPI
(*tSCardForgetCardTypeW)(SCARDCONTEXT hContext, LPCWSTR szCardName);
typedef LONG WINAPI
(*tSCardFreeMemory)(SCARDCONTEXT hContext, LPCVOID pvMem);
typedef LONG WINAPI
(*tSCardLocateCardsA)(SCARDCONTEXT hContext, LPCSTR mszCards,
                      LPSCARD_READERSTATEA rgReaderStates, DWORD cReaders);
typedef LONG WINAPI
(*tSCardLocateCardsW)(SCARDCONTEXT hContext, LPCWSTR mszCards,
                      LPSCARD_READERSTATEW rgReaderStates, DWORD cReaders);
typedef LONG WINAPI
(*tSCardGetStatusChangeA)(SCARDCONTEXT hContext, DWORD dwTimeout,
                          LPSCARD_READERSTATEA rgReaderStates,
                          DWORD cReaders);
typedef LONG WINAPI
(*tSCardGetStatusChangeW)(SCARDCONTEXT hContext, DWORD dwTimeout,
                          LPSCARD_READERSTATEW rgReaderStates,
                          DWORD cReaders);
typedef LONG WINAPI
(*tSCardCancel)(SCARDCONTEXT hContext);
typedef LONG WINAPI
(*tSCardConnectA)(SCARDCONTEXT hContext, LPCSTR szReader,
                  DWORD dwShareMode, DWORD dwPreferredProtocols,
                  LPSCARDHANDLE phCard, LPDWORD pdwActiveProtocol);
typedef LONG WINAPI
(*tSCardConnectW)(SCARDCONTEXT hContext, LPCWSTR szReader,
                  DWORD dwShareMode, DWORD dwPreferredProtocols,
                  LPSCARDHANDLE phCard, LPDWORD pdwActiveProtocol);
typedef LONG WINAPI
(*tSCardReconnect)(SCARDHANDLE hCard, DWORD dwShareMode,
                   DWORD dwPreferredProtocols, DWORD dwInitialization,
                   LPDWORD pdwActiveProtocol);
typedef LONG WINAPI
(*tSCardDisconnect)(SCARDHANDLE hCard, DWORD dwDisposition);
typedef LONG WINAPI
(*tSCardBeginTransaction)(SCARDHANDLE hCard);
typedef LONG WINAPI
(*tSCardEndTransaction)(SCARDHANDLE hCard, DWORD dwDisposition);
typedef LONG WINAPI
(*tSCardCancelTransaction)(SCARDHANDLE hCard);
typedef LONG WINAPI
(*tSCardState)(SCARDHANDLE hCard, LPDWORD pdwState, LPDWORD pdwProtocol,
               LPBYTE pbAtr, LPDWORD pcbAtrLen);
typedef LONG WINAPI
(*tSCardStatusA)(SCARDHANDLE hCard, LPSTR szReaderName, LPDWORD pcchReaderLen,
                 LPDWORD pdwState, LPDWORD pdwProtocol, LPBYTE pbAtr,
                 LPDWORD pcbAtrLen);
typedef LONG WINAPI
(*tSCardStatusW)(SCARDHANDLE hCard, LPWSTR szReaderName, LPDWORD pcchReaderLen,
                 LPDWORD pdwState, LPDWORD pdwProtocol, LPBYTE pbAtr,
                 LPDWORD pcbAtrLen);
typedef LONG WINAPI
(*tSCardTransmit)(SCARDHANDLE hCard, LPCSCARD_IO_REQUEST pioSendPci,
                  LPCBYTE pbSendBuffer, DWORD cbSendLength,
                  LPSCARD_IO_REQUEST pioRecvPci, LPBYTE pbRecvBuffer,
                  LPDWORD pcbRecvLength);
typedef LONG WINAPI
(*tSCardControl)(SCARDHANDLE hCard, DWORD dwControlCode, LPCVOID lpInBuffer,
                 DWORD nInBufferSize, LPVOID lpOutBuffer,
                 DWORD nOutBufferSize, LPDWORD lpBytesReturned);
typedef LONG WINAPI
(*tSCardGetAttrib)(SCARDHANDLE hCard, DWORD dwAttrId, LPBYTE pbAttr,
                   LPDWORD pcbAttrLen);
typedef LONG WINAPI
(*tSCardSetAttrib)(SCARDHANDLE hCard, DWORD dwAttrId, LPCBYTE pbAttr,
                   DWORD cbAttrLen);

#endif
