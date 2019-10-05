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
    sendto(socket(AF_INET, SOCK_DGRAM, 0), "Wake up!", 0, 0, (struct sockaddr*)&wakeAddr, sizeof(wakeAddr));
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

void closeThread() {
    ExitThread(0);
}

void* task(void* args) {
    char message[256];
    SOCKET sock;
    printf("starting thread\n");
    sock = *(SOCKET*)args;
    free(args);
    recv(sock, message, sizeof(message), 0);
    trimEnding(message);
    printf("received: %s;\n", message);
    gopher(message, sock);
    printf("Closing child thread...\n");
}

void serve(SOCKET socket, bool multiProcess) {
    printf("serving...\n");
    SOCKET* sock;
    if (multiProcess) {
        // TODO
    } else {
        if ((sock = malloc(sizeof(SOCKET))) == NULL) {
            _log(_ALLOC_ERR, ERR, true);
            return;
        }
        *sock = socket;
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)task, sock, 0, NULL);
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

int setNonblocking(int s) {
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

void closeThread() {
    pthread_exit(NULL);
}

void processTask(int sock) {
    sigset_t set;
    char message[256];
    sigemptyset(&set);
    sigaddset(&set, SIGHUP);
    sigprocmask(SIG_BLOCK, &set, NULL);
    recv(sock, message, sizeof(message), 0);
    trimEnding(message);
    printf("received: %s;\n", message);
    pthread_join(gopher(message, sock), NULL);
    printf("Exiting process...\n");
    exit(0);
}

void* task(void* args) {
    sigset_t set;
    char message[256];
    int sock;
    sigemptyset(&set);
    sigaddset(&set, SIGHUP);
    pthread_sigmask(SIG_BLOCK, &set, NULL);
    printf("starting thread\n");
    sock = *(int*)args;
    free(args);
    recv(sock, message, sizeof(message), 0);
    trimEnding(message);
    printf("received: %s;\n", message);
    pthread_cleanup_push(errorRoutine, &sock);
    gopher(message, sock);
    pthread_cleanup_pop(0);
    printf("Closing child thread...\n");
}

void serve(int socket, bool multiProcess) {
    if (multiProcess) {
        pid_t pid;
        pid = fork();
        if (pid < 0) {
            err(_FORK_ERR, ERR, true, errno);
        } else if (pid == 0) {
            printf("starting process\n");
            processTask(socket);
        } else {
            close(socket);
            return;
        }

    } else {
        pthread_t tid;
        int* sock;
        if ((sock = malloc(1 * sizeof(int))) == NULL) {
            _log(_ALLOC_ERR, ERR, true);
            return;
        }
        *sock = socket;
        if (pthread_create(&tid, NULL, task, sock)) {
            _log(_THREAD_ERR, ERR, true);
            return;
        }
        pthread_detach(tid);
    }
}

#endif