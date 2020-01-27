#include <stdio.h>
#include <windows.h>
#include "../../headers/logger.h"
#include "../../headers/protocol.h"

/* Legge la richiesta ed esegue il protocollo Gopher */
DWORD main(DWORD argc, LPSTR* argv) {
    SOCKET sock = INVALID_SOCKET;
    int port = -1;
    mutex_t* pMutex = NULL;
    logger_t* pLogger = NULL;
    HANDLE logEvent = NULL;
    HANDLE logPipe = NULL;
    WSADATA wsaData;
    WORD versionWanted = MAKEWORD(1, 1);
    int step = 0;
    if (WSAStartup(versionWanted, &wsaData) != 0) {
        goto ON_ERROR;
    }
    sscanf(argv[1], "%hu", &port);
    sscanf(argv[2], "%p", &sock);
    sscanf(argv[3], "%p", &(logPipe));
    pMutex = malloc(sizeof(mutex_t));
    if (NULL == pMutex) {
        goto ON_ERROR;
    }
    *pMutex = OpenMutex(SYNCHRONIZE, FALSE, LOG_MUTEX_NAME);
    logEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, LOGGER_EVENT_NAME);
    if (!(*pMutex && !logEvent && !logPipe)) {
        pLogger = NULL;
    } else {
        pLogger = malloc(sizeof(logger_t));
        if (NULL == pLogger) {
            goto ON_ERROR;
        }
        pLogger->logEvent = logEvent;
        pLogger->logPipe = logPipe;
        pLogger->pLogMutex = *pMutex;
    }
    gopher(sock, port, pLogger);
    ExitThread(0);
ON_ERROR:
    if (INVALID_SOCKET != sock) {
        closesocket(sock);
    }
    WSACleanup();
    if (pLogger) {
        free(pLogger);
    }
    if (pMutex) {
        free(pMutex);
    }
    return 1;
}