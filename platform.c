#include "platform.h"

#if defined(_WIN32)   /* Windows functions */
#include <windows.h>

void startup() {
    int errNo;
    WORD versionWanted = MAKEWORD(1, 1);
    WSADATA wsaData;
    if ((errNo = WSAStartup(versionWanted, &wsaData)) != 0) {
        err("Errore nell'inizializzazione di Winsock", ERR, errNo);
    }
}

#else    /* Linux functions */
#include <unistd.h>
#include <sys/socket>

void startup() {}

#endif /*  Common functions */

_socket openSocket(int domain, int type, int protocol) {
    return socket(domain, type, protocol);
}