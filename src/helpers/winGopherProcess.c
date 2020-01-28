#include <stdio.h>
#include <windows.h>
#include "../../headers/logger.h"
#include "../../headers/protocol.h"

static int getLogger(logger_t* logger, LPSTR* argv) {
    mutex_t* pMutex = NULL;
    logger_t* pLogger = NULL;
    HANDLE logEvent = NULL;
    HANDLE logPipe = NULL;
    sscanf(argv[3], "%p", &(logPipe));
    pMutex = malloc(sizeof(mutex_t));
    if (NULL == pMutex) {
        return LOGGER_FAILURE;
    }
    *pMutex = OpenMutex(SYNCHRONIZE, FALSE, LOG_MUTEX_NAME);
    logEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, LOGGER_EVENT_NAME);
    if (!(*pMutex && !logEvent && !logPipe)) {
        return LOGGER_FAILURE;
    } else {
        pLogger = malloc(sizeof(logger_t));
        if (NULL == pLogger) {
            return LOGGER_FAILURE;
        }
        pLogger->logEvent = logEvent;
        pLogger->logPipe = logPipe;
        pLogger->pLogMutex = *pMutex;
    }
    return LOGGER_SUCCESS;
ON_ERROR:
    if (pMutex) {
        free(pMutex);
    }
    if (pLogger) {
        free(pLogger);
    }
    if (logEvent) {
        CloseHandle(logEvent);
    }
    if (logPipe) {
        CloseHandle(logPipe);
    }
    return LOGGER_FAILURE;
}

/* Legge la richiesta ed esegue il protocollo Gopher */
DWORD main(DWORD argc, LPSTR* argv) {
    SOCKET sock = INVALID_SOCKET;
    int port = -1;
    logger_t* pLogger;
    WSADATA wsaData;
    WORD versionWanted = MAKEWORD(1, 1);
    int step = 0;
    if (WSAStartup(versionWanted, &wsaData) != 0) {
        return 1;
    }
    sscanf(argv[1], "%hu", &port);
    sscanf(argv[2], "%p", &sock);
    if (LOGGER_SUCCESS != getLogger(pLogger, argv)) {
        pLogger = NULL;
    }
    gopher(sock, port, pLogger);
    ExitThread(0);
}