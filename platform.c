#include "includes/platform.h"

bool sig = false;
_socket wakeSelect;
struct sockaddr_in wakeAddr;

#if defined(_WIN32)

/*****************************************************************************************************************/
/*                                             WINDOWS FUNCTIONS                                                 */

/*****************************************************************************************************************/

/************************************************** UTILS ********************************************************/

void errorString(char* error) {
    sprintf(error, "Error: %d, Socket error: %d", GetLastError(), WSAGetLastError());
}

/********************************************** SOCKETS *************************************************************/

int startup() {
    WORD versionWanted = MAKEWORD(1, 1);
    WSADATA wsaData;
    return WSAStartup(versionWanted, &wsaData);
}

int sockErr() {
    return WSAGetLastError();
}

int setNonblocking(SOCKET s) {
    unsigned long blocking = 1;
    return ioctlsocket(s, FIONBIO, &blocking);
}

int closeSocket(SOCKET s) {
    return closesocket(s);
}

/********************************************** SIGNALS *************************************************************/

// TODO ???
BOOL ctrlBreak(DWORD sign) {
    if (sign == CTRL_BREAK_EVENT) {
        printf("Richiesta chiusura");
        exit(0);
    }
}

BOOL sigHandler(DWORD signum) {
    printf("segnale ricevuto\n");
    sendto(socket(AF_INET, SOCK_DGRAM, 0), "", 0, 0, (struct sockaddr*)&wakeAddr, sizeof(wakeAddr));
    sig = true;
    return signum == CTRL_C_EVENT;
}

void installSigHandler() {
    wakeSelect = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&wakeAddr, 0, sizeof(wakeAddr));
    wakeAddr.sin_family = AF_INET;
    wakeAddr.sin_port = htons(49152);
    wakeAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    bind(wakeSelect, (struct sockaddr*)&wakeAddr, sizeof(wakeAddr));
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)ctrlBreak, true);
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)sigHandler, true);
}

/*********************************************** MULTI ***************************************************************/

void serve(_socket socket, bool multiProcess) {
    HANDLE thread;
    _socket* sock;
    if (multiProcess) {
        // TODO
    } else {
        if ((sock = malloc(1 * sizeof(_socket))) == NULL) {
            err(_ALLOC_ERR, ERR, true, -1);
        }
        *sock = socket;
        thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)task, sock, 0, NULL);
    }
}

#else

/*****************************************************************************************************************/
/*                                             UNIX FUNCTIONS                                                    */

/*****************************************************************************************************************/

/************************************************** UTILS ********************************************************/

void errorString(char* error) {
    sprintf(error, "%s", strerror(errno));
}

/********************************************** SOCKETS *************************************************************/

int startup() {}

int sockErr() {
    return errno;
}

int setNonblocking(_socket s) {
    return fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
}

int closeSocket(int s) {
    return close(s);
}

/********************************************** SIGNALS *************************************************************/

void sigHandler(int signum) {
    printf("Registrato segnale \n");
    sig = true;
}

void installSigHandler() {
    struct sigaction sHup;
    struct sigaction sInt;
    sHup.sa_handler = sigHandler;
    sigemptyset(&sHup.sa_mask);
    sHup.sa_flags = SA_NODEFER;
    sigaction(SIGHUP, &sHup, NULL);
}

/*********************************************** MULTI ***************************************************************/

void serve(_socket socket, bool multiProcess) {
    pthread_t tid;
    pid_t pid;
    struct threadArgs args;
    if (multiProcess) {
        pid = fork();
        if (pid < 0) {
            err(_FORK_ERR, ERR, true, errno);
        } else if (pid == 0) {
            return;
        } else {
            // TODO
        }

    } else {
        args.socket = socket;
        args.options = options;
        if (pthread_create(&tid, NULL, task, &args)) {
            err(_THREAD_ERR, ERR, true, -1);
        }
        pthread_detach(tid);
    }
}

#endif
