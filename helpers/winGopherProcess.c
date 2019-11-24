#include "../headers/gopher.h"

/* Legge la richiesta ed esegue il protocollo Gopher */
DWORD main(DWORD argc, LPSTR* argv) {
    SOCKET sock;
    HANDLE thread;
    WSADATA wsaData;
    WORD versionWanted = MAKEWORD(1, 1);
    WSAStartup(versionWanted, &wsaData);
    sscanf(argv[1], "%d", &sock);
    sscanf(argv[2], "%p", &logPipe);
    sscanf(argv[3], "%p", &logEvent);
    thread = gopher(sock);
    if (thread != NULL) {
        WaitForSingleObject(thread, 10000);
    }
    closeSocket(sock);
    return 0;
}