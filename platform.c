#include "platform.h"

#if defined(_WIN32)
#include <windows.h>

void startup() {
    /* Windows Socket warmup */
    int errNo;
    WORD versionWanted = MAKEWORD(1, 1);
    WSADATA wsaData;
    if ((errNo = WSAStartup(versionWanted, &wsaData)) != 0) {
        err("Errore nell'inizializzazione di Winsock", errNo);
    }
}



#else
#include <unistd.h>

void startup() {}

#endif