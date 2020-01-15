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
    if (WSAStartup(versionWanted, &wsaData) != 0) {
        return -1;
    }
    atexit(cleanup);
    sscanf(argv[1], "%hu", &port);
    sscanf(argv[2], "%p", &sock);
    sscanf(argv[3], "%p", &logPipe);
    sscanf(argv[4], "%p", &logEvent);
    // TODO handling
    gopher(sock, port);
    ExitThread(0);
}