#include "../headers/gopher.h"

void cleanup() {
    WSACleanup();
}

/* Legge la richiesta ed esegue il protocollo Gopher */
DWORD main(DWORD argc, LPSTR* argv) {
    SOCKET sock;
    unsigned short port;
    WSADATA wsaData;
    WORD versionWanted = MAKEWORD(1, 1);
    _mutex mutex;
    if (WSAStartup(versionWanted, &wsaData) != 0) {
        return GOPHER_FAILURE;
    }
    if ((mutex = OpenMutex(SYNCHRONIZE, FALSE, LOG_MUTEX_NAME)) == NULL) {
        return GOPHER_FAILURE;
    }
    atexit(cleanup);
    sscanf(argv[1], "%hu", &port);
    sscanf(argv[2], "%p", &sock);
    sscanf(argv[3], "%p", &logPipe);
    sscanf(argv[4], "%p", &logEvent);
    gopher(sock, port);
    ExitThread(0);
}