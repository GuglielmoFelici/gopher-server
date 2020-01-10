#include "../headers/gopher.h"

/* Legge la richiesta ed esegue il protocollo Gopher */
DWORD main(DWORD argc, LPSTR* argv) {
    SOCKET sock;
    unsigned short port;
    HANDLE thread;
    WSADATA wsaData;
    WORD versionWanted = MAKEWORD(1, 1);
    if (WSAStartup(versionWanted, &wsaData) != 0) {
        return -1;
    }
    sscanf(argv[1], "%d", &sock);
    sscanf(argv[2], "%hu", &port);
    sscanf(argv[3], "%p", &logPipe);
    sscanf(argv[4], "%p", &logEvent);
    gopher(sock, true, port);
    closeSocket(sock);
    return WSACleanup();
}