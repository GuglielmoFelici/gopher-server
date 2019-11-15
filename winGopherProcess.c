#include "includes/gopher.h"
DWORD main(DWORD argc, LPSTR* argv) {
    SOCKET sock;
    HANDLE thread;
    char message[256];
    startup();
    sock = atoi(argv[1]);
    recv(sock, message, sizeof(message), 0);
    trimEnding(message);
    printf("wingopher - request: %s\n", message);
    thread = gopher(message, sock);
    if (thread != NULL) {
        WaitForSingleObject(thread, 10000);
    }
    closeSocket(sock);
    printf("Exiting process...\n");
    return 0;
}