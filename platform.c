#include "includes/gopher.h"

#if defined(_WIN32)

/*****************************************************************************************************************/
/*                                             WINDOWS FUNCTIONS                                                 */

/*****************************************************************************************************************/

/************************************************** UTILS ********************************************************/

void errorString(char* error) {
    sprintf(error, "Error: %d, Socket error: %d", GetLastError(), WSAGetLastError());
}

void _shutdown() {
    closeSocket(server);
    AttachConsole(logger);
    GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, logger);
    printf("All done.\n");
    exit(0);
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

void wakeUpServer() {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    sendto(s, "Wake up!", 0, 0, (struct sockaddr*)&wakeAddr, sizeof(wakeAddr));
    closeSocket(s);
}

BOOL ctrlBreak(DWORD signum) {
    if (signum == CTRL_BREAK_EVENT) {
        printf("Richiesta chiusura\n");
        requestShutdown = true;
        wakeUpServer();
        return true;
    }
    return false;
}

BOOL sigHandler(DWORD signum) {
    if (signum != CTRL_BREAK_EVENT) {
        printf("segnale ricevuto\n");
        signaled = true;
        wakeUpServer();
        return true;
    }
    return false;
}

void installSigHandler() {
    wakeSelect = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&wakeAddr, 0, sizeof(wakeAddr));
    wakeAddr.sin_family = AF_INET;
    wakeAddr.sin_port = htons(49152);
    wakeAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    bind(wakeSelect, (struct sockaddr*)&wakeAddr, sizeof(wakeAddr));
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)ctrlBreak, TRUE);
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)sigHandler, TRUE);
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

void serveThread(SOCKET* sock) {
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)task, sock, 0, NULL);
}

void serveProc(SOCKET client) {
    STARTUPINFO startupInfo;
    PROCESS_INFORMATION processInfo;
    memset(&startupInfo, 0, sizeof(startupInfo));
    memset(&processInfo, 0, sizeof(processInfo));
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startupInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startupInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    char cmdLine[sizeof("winGopherProcess.exe ") + sizeof(SOCKET)];  // TODO size??
    sprintf(cmdLine, "winGopherProcess.exe %d", client);
    CreateProcess("winGopherProcess.exe", cmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &startupInfo, &processInfo);
}

/*********************************************** LOGGER ***************************************************************/

void logTransfer(LPSTR log) {
    // TODO mutex
    DWORD written;
    WriteFile(logPipe, log, strlen(log), &written, NULL);
    SetEvent(logEvent);
}

void startTransferLog() {
    HANDLE readPipe;
    LPSECURITY_ATTRIBUTES attr;
    STARTUPINFO startupInfo;
    PROCESS_INFORMATION processInfo;
    attr->bInheritHandle = TRUE;
    attr->nLength = sizeof(attr);
    attr->lpSecurityDescriptor = NULL;
    if (!CreatePipe(&readPipe, &logPipe, attr, 0)) {
        err("startTransferLog() - Impossibile creare la pipe", ERR, true, -1);
    }
    memset(&startupInfo, 0, sizeof(startupInfo));
    memset(&processInfo, 0, sizeof(processInfo));
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdInput = readPipe;
    startupInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startupInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    if ((logEvent = CreateEvent(attr, FALSE, FALSE, "logEvent")) == NULL) {
        err("startTransferLog() - Impossibile creare l'evento", ERR, true, -1);
    }
    if (!CreateProcess("winLogger.exe", NULL, NULL, NULL, TRUE, 0, NULL, NULL, &startupInfo, &processInfo)) {
        err("startTransferLog() - Impossibile avviare il logger", ERR, true, -1);
    }
    logger = processInfo.dwProcessId;
    CloseHandle(readPipe);
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

// Demonizzazione
int startup() {
    int pid;
    pid = fork();
    if (pid < 0) {
        err(_DAEMON_ERR, ERR, true, errno);
    } else if (pid > 0) {
        exit(0);
    } else {
        sigset_t set;
        if (setsid() < 0) {
            err(_DAEMON_ERR, ERR, true, errno);
        }
        sigemptyset(&set);
        sigaddset(&set, SIGHUP);
        sigprocmask(SIG_BLOCK, &set, NULL);
        pid = fork();
        if (pid < 0) {
            err(_DAEMON_ERR, ERR, true, errno);
        } else if (pid > 0) {
            exit(0);
        } else {
            int devNull;
            devNull = open("/dev/null", O_RDWR);
            if (dup2(devNull, STDIN_FILENO) < 0 || dup2(devNull, STDOUT_FILENO) < 0 || dup2(devNull, STDERR_FILENO) < 0) {
                err(_DAEMON_ERR, ERR, true, -1);
            }
            return close(devNull);
        }
    }
    return 0;
}

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
    signaled = true;
}

void installSigHandler() {
    struct sigaction sHup;
    sHup.sa_handler = sigHandler;
    sigemptyset(&sHup.sa_mask);
    sHup.sa_flags = SA_NODEFER;
    sigaction(SIGHUP, &sHup, NULL);
}

/*********************************************** MULTI ***************************************************************/

void closeThread() {
    pthread_exit(NULL);
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
    fflush(stdout);
}

void serveThread(int* sock) {
    pthread_t tid;
    if (pthread_create(&tid, NULL, task, sock)) {
        _log(_THREAD_ERR, ERR, true);
        return;
    }
    pthread_detach(tid);
}

void serveProc(int sock) {
    pid_t pid;
    pid = fork();
    if (pid < 0) {
        err(_FORK_ERR, ERR, true, errno);
    } else if (pid == 0) {
        char message[256];
        printf("starting process\n");
        recv(sock, message, sizeof(message), 0);
        trimEnding(message);
        printf("received: %s;\n", message);
        pthread_cleanup_push(errorRoutine, &sock);
        pthread_join(gopher(message, sock), NULL);
        pthread_cleanup_pop(0);
        printf("Exiting process...\n");
        exit(0);
    }
}

/*********************************************** LOGGER ***************************************************************/

pthread_mutex_t* mutexShare;
pthread_cond_t* condShare;

void logTransfer(char* log) {
    pthread_mutex_lock(mutexShare);
    write(logPipe, log, strlen(log));
    pthread_cond_signal(condShare);
    pthread_mutex_unlock(mutexShare);
}

void startTransferLog() {
    int pid;
    int pipeFd[2];
    pthread_mutex_t mutex;
    pthread_mutexattr_t mutexAttr;
    pthread_cond_t cond;
    pthread_condattr_t condAttr;
    pthread_mutexattr_init(&mutexAttr);
    pthread_mutexattr_setpshared(&mutexAttr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&mutex, &mutexAttr);
    pthread_condattr_init(&condAttr);
    pthread_condattr_setpshared(&condAttr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&cond, &condAttr);
    mutexShare = mmap(NULL, sizeof(pthread_mutex_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (mutexShare == MAP_FAILED) {
        err("startTransferLog() - impossibile mappare il mutex in memoria", ERR, true, -1);
    }
    *mutexShare = mutex;
    condShare = mmap(NULL, sizeof(pthread_cond_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (condShare == MAP_FAILED) {
        err("startTransferLog() - impossibile mappare la condition variable in memoria", ERR, true, -1);
    }
    *condShare = cond;
    if (pipe2(pipeFd, O_DIRECT) < 0) {
        err("startTransferLog() - impossibile aprire la pipe", ERR, true, -1);
    }
    if ((pid = fork()) < 0) {
        err(_LOG_ERR, ERR, true, pid);
    } else if (pid == 0) {
        int logFile;
        char buff[PIPE_BUF + 1];
        prctl(PR_SET_NAME, "Logger");
        close(pipeFd[1]);
        logPipe = pipeFd[0];
        if ((logFile = open("logFile", O_WRONLY | O_CREAT | O_APPEND, S_IRWXU)) < 0) {
            exit(1);
        }
        pthread_mutex_lock(mutexShare);
        while (1) {
            size_t bytesRead;
            pthread_cond_wait(condShare, mutexShare);
            printf("entrato in condition variable\n");
            if ((bytesRead = read(logPipe, buff, PIPE_BUF)) < 0) {
                close(logFile);
                exit(1);
            } else if (!strcmp(buff, "KILL")) {
                break;
            } else {
                write(logFile, strcat(buff, "\n"), bytesRead + 1);
            }
        }
        munmap(mutexShare, sizeof(pthread_mutex_t));
        munmap(condShare, sizeof(pthread_cond_t));
        free(mutexShare);
        free(condShare);
        close(logPipe);
        close(logFile);
    } else {
        close(pipeFd[0]);
        logPipe = pipeFd[1];
    }
}

#endif