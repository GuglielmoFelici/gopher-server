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
    WSADATA wsaData;
    WORD versionWanted = MAKEWORD(1, 1);
    if (WSAStartup(versionWanted, &wsaData) != 0) {
        goto ON_ERROR;
    }
    pMutex = malloc(sizeof(mutex_t));
    if (NULL == pMutex) {
        goto ON_ERROR;
    }
    if (NULL == (*pMutex = OpenMutex(SYNCHRONIZE, FALSE, LOG_MUTEX_NAME))) {
        goto ON_ERROR;
    }
    pLogger = malloc(sizeof(logger_t));
    if (NULL == pLogger) {
        goto ON_ERROR;
    }
    pLogger->pLogMutex = pMutex;
    sscanf(argv[1], "%hu", &port);
    sscanf(argv[2], "%p", &sock);
    sscanf(argv[3], "%p", &(pLogger->logPipe));
    sscanf(argv[4], "%p", &(pLogger->logEvent));
    gopher(sock, port, pLogger);
    ExitThread(0);
ON_ERROR:
    WSACleanup();
    if (pLogger) {
        free(pLogger);
    }
    if (pMutex) {
        free(pMutex);
    }
    return 1;
}