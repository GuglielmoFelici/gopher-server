#include "includes/platform.h"

#if defined(_WIN32)   /* Windows functions */
#include <windows.h>

int startup() {
    WORD versionWanted = MAKEWORD(1, 1);
    WSADATA wsaData;
    return WSAStartup(versionWanted, &wsaData);
}

#else    /* Linux functions */
#include <unistd.h>
#include <sys/socket>

int startup() {}

#endif /*  Common functions */

_socket openSocket(int domain, int type, int protocol) {
    return socket(domain, type, protocol);
}