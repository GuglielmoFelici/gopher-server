#include <stdio.h>
#include <windows.h>

#include "../../headers/logger.h"
#include "../../headers/protocol.h"

static int getLogger(logger_t* pLogger, LPSTR* argv) {
    HANDLE mutex = NULL;
    HANDLE logEvent = NULL;
    HANDLE logPipe = NULL;
    if (NULL == pLogger) {
        return LOGGER_FAILURE;
    }
    sscanf(argv[3], "%p", &(logPipe));
    mutex = OpenMutex(SYNCHRONIZE, FALSE, LOG_MUTEX_NAME);
    logEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, LOGGER_EVENT_NAME);
    if (!(mutex && logEvent && logPipe)) {
        return LOGGER_FAILURE;
    } else {
        pLogger->logPipe = logPipe;
        pLogger->logEvent = logEvent;
        pLogger->pLogMutex = malloc(sizeof(HANDLE));
        if (NULL == pLogger->pLogMutex) {
            return LOGGER_FAILURE;
        }
        *(pLogger->pLogMutex) = mutex;
    }
    return LOGGER_SUCCESS;
ON_ERROR:
    if (pLogger && pLogger->pLogMutex) {
        free(pLogger->pLogMutex);
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
    logger_t* pLogger = NULL;
    WSADATA wsaData;
    WORD versionWanted = MAKEWORD(1, 1);
    int step = 0;
    if (WSAStartup(versionWanted, &wsaData) != 0) {
        return 1;
    }
    sscanf(argv[1], "%hu", &port);
    sscanf(argv[2], "%p", &sock);
    pLogger = malloc(sizeof(logger_t));
    if (pLogger == NULL) {
        return 1;
    }
    if (LOGGER_SUCCESS != getLogger(pLogger, argv)) {
        pLogger = NULL;
    }
    if (GOPHER_SUCCESS == gopher(sock, port, pLogger)) {
        ExitThread(0);
    }
}