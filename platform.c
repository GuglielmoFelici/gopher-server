#include "headers/gopher.h"

#if defined(_WIN32)

/*****************************************************************************************************************/
/*                                             WINDOWS FUNCTIONS                                                 */

/*****************************************************************************************************************/

/************************************************** UTILS ********************************************************/

void errorString(char *error, size_t size) {
    snprintf(error, size, "Error: %d, Socket error: %d", GetLastError(), WSAGetLastError());
}

/* Termina graziosamente il logger, poi termina il server. */
void _shutdown() {
    closeSocket(server);
    AttachConsole(loggerId);
    GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, loggerId);
    printf("All done.\n");
    exit(0);
}

/********************************************** SOCKETS *************************************************************/

/* Inizializza la WSA */
int startup() {
    WORD versionWanted = MAKEWORD(1, 1);
    WSADATA wsaData;
    return WSAStartup(versionWanted, &wsaData);
}

/* Ritorna l'ultimo codice di errore relativo alle chiamate WSA */
// TODO eliminare?
int sockErr() {
    return WSAGetLastError();
}

int closeSocket(SOCKET s) {
    return closesocket(s);
}

/********************************************** SIGNALS *************************************************************/

/* Invia un messaggio vuoto al socket awakeSelect per interrompere la select del server. */
void wakeUpServer() {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    sendto(s, "Wake up!", 0, 0, (struct sockaddr *)&awakeAddr, sizeof(awakeAddr));
    closeSocket(s);
}

/* Termina graziosamente il programma. */
BOOL ctrlC(DWORD signum) {
    if (signum == CTRL_C_EVENT) {
        printf("Richiesta chiusura\n");
        requestShutdown = true;
        wakeUpServer();
        return true;
    }
    return false;
}

/* Richiede la rilettura del file di configurazione */
BOOL sigHandler(DWORD signum) {
    if (signum != CTRL_C_EVENT) {
        updateConfig = true;
        wakeUpServer();
        return true;
    }
    return false;
}

/* Installa i gestori di eventi console */
void installSigHandler() {
    awakeSelect = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&awakeAddr, 0, sizeof(awakeAddr));
    awakeAddr.sin_family = AF_INET;
    awakeAddr.sin_port = htons(49152);
    awakeAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    bind(awakeSelect, (struct sockaddr *)&awakeAddr, sizeof(awakeAddr));
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)ctrlC, TRUE);
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)sigHandler, TRUE);
}

/*********************************************** THREADS & PROCESSES ***************************************************************/

void closeThread() {
    ExitThread(0);
}

/* Task lanciato dal server per avviare un thread che esegue il protocollo Gopher. */
void *serveThreadTask(void *args) {
    char message[256];
    SOCKET sock;
    printf("Starting new thread\n");
    sock = *(SOCKET *)args;
    free(args);
    recv(sock, message, sizeof(message), 0);
    trimEnding(message);
    printf("Request: %s;\n", message);
    gopher(message, sock);  // Il protocollo viene eseguito qui
}

/* Serve una richiesta in modalità multithreading. */
void serveThread(SOCKET *sock) {
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)serveThreadTask, sock, 0, NULL);
}

/* Serve una richiesta in modalità multiprocesso. */
void serveProc(SOCKET client) {
    _string exec = "helpers/winGopherProcess.exe";
    STARTUPINFO startupInfo;
    PROCESS_INFORMATION processInfo;
    char cmdLine[sizeof("helpers/winGopherProcess.exe ") + sizeof(SOCKET)];  // TODO size??
    memset(&startupInfo, 0, sizeof(startupInfo));
    memset(&processInfo, 0, sizeof(processInfo));
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startupInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startupInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    snprintf(cmdLine, sizeof(cmdLine), "%s %d", exec, client);
    printf(cmdLine);
    CreateProcess(exec, cmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &startupInfo, &processInfo);
}

/*********************************************** LOGGER ***************************************************************/

void logTransfer(LPSTR log) {
    // TODO mutex
    DWORD written;
    WriteFile(logPipe, log, strlen(log), &written, NULL);
    SetEvent(logEvent);
}

/* Avvia il processo di logging dei trasferimenti. */
void startTransferLog() {
    _string exec = "helpers/winLogger.exe";
    HANDLE readPipe;
    SECURITY_ATTRIBUTES attr;
    STARTUPINFO startupInfo;
    PROCESS_INFORMATION processInfo;
    memset(&attr, 0, sizeof(attr));
    attr.bInheritHandle = TRUE;
    attr.nLength = sizeof(attr);
    attr.lpSecurityDescriptor = NULL;
    // logPipe è globale, viene acceduta in scrittura quando avviene un trasferimento file
    if (!CreatePipe(&readPipe, &logPipe, &attr, 0)) {
        _err("startTransferLog() - Impossibile creare la pipe", ERR, true, -1);
    }
    memset(&startupInfo, 0, sizeof(startupInfo));
    memset(&processInfo, 0, sizeof(processInfo));
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdInput = readPipe;
    startupInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startupInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    // Utilizzato per notificare al logger che ci sono dati da leggere sulla pipe
    if ((logEvent = CreateEvent(&attr, FALSE, FALSE, "logEvent")) == NULL) {
        _err("startTransferLog() - Impossibile creare l'evento", ERR, true, -1);
    }
    if (!CreateProcess(exec, NULL, NULL, NULL, TRUE, 0, NULL, NULL, &startupInfo, &processInfo)) {
        _err("startTransferLog() - Impossibile avviare il logger", ERR, true, -1);
    }
    loggerId = processInfo.dwProcessId;
    CloseHandle(readPipe);
}

#else

/*****************************************************************************************************************/
/*                                             UNIX FUNCTIONS                                                    */

/*****************************************************************************************************************/

/************************************************** UTILS ********************************************************/

void _shutdown() {
    closeSocket(server);
    printf("All done.\n");
    exit(0);
}

void errorString(char *error, size_t size) {
    snprintf(error, size, "%s", strerror(errno));
}

/********************************************** SOCKETS *************************************************************/

/* Rende il server un processo demone */
int startup() {
    int pid;
    pid = fork();
    if (pid < 0) {
        _err(_DAEMON_ERR, ERR, true, errno);
    } else if (pid > 0) {
        exit(0);
    } else {
        sigset_t set;
        if (setsid() < 0) {
            _err(_DAEMON_ERR, ERR, true, errno);
        }
        sigemptyset(&set);
        sigaddset(&set, SIGHUP);
        sigprocmask(SIG_BLOCK, &set, NULL);
        pid = fork();
        if (pid < 0) {
            _err(_DAEMON_ERR, ERR, true, errno);
        } else if (pid > 0) {
            exit(0);
        } else {
            int devNull;
            devNull = open("/dev/null", O_RDWR);
            if (dup2(devNull, STDIN_FILENO) < 0 || dup2(devNull, STDOUT_FILENO) < 0 || dup2(devNull, STDERR_FILENO) < 0) {
                _err(_DAEMON_ERR, ERR, true, -1);
            }
            return close(devNull);
        }
    }
    return 0;
}

int sockErr() {
    return errno;
}

int closeSocket(int s) {
    return close(s);
}

/********************************************** SIGNALS *************************************************************/

/* Richiede la rilettura del file di configurazione */
void sigHandler(int signum) {
    updateConfig = true;
}

/* Richiede la terminazione */
void intHandler(int signum) {
    requestShutdown = true;
    if (getpid() != loggerId) {
        logTransfer("");
    }
}

/* Installa i gestori di segnali */
void installSigHandler() {
    struct sigaction sHup;
    struct sigaction sInt;
    sHup.sa_handler = sigHandler;
    sInt.sa_handler = intHandler;
    sigemptyset(&sHup.sa_mask);
    sigemptyset(&sInt.sa_mask);
    sInt.sa_flags = 0;
    sHup.sa_flags = SA_NODEFER;
    sigaction(SIGHUP, &sHup, NULL);
    sigaction(SIGINT, &sInt, NULL);
}

/*********************************************** MULTI ***************************************************************/

void closeThread() {
    pthread_exit(NULL);
}

/* Task lanciato dal server per avviare un thread che esegue il protocollo Gopher. */
void *serveThreadTask(void *args) {
    sigset_t set;
    char message[256];
    int sock;
    sigemptyset(&set);
    sigaddset(&set, SIGHUP);
    pthread_sigmask(SIG_BLOCK, &set, NULL);
    printf("starting thread\n");
    sock = *(int *)args;
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

/* Serve una richiesta in modalità multithreading. */
void serveThread(int *sock) {
    pthread_t tid;
    if (pthread_create(&tid, NULL, serveThreadTask, sock)) {
        printf(_THREAD_ERR);
        return;
    }
    pthread_detach(tid);
}

/* Serve una richiesta in modalità multiprocesso. */
void serveProc(int sock) {
    pid_t pid;
    pid = fork();
    if (pid < 0) {
        _err(_FORK_ERR, ERR, true, errno);
    } else if (pid == 0) {
        char message[256];
        printf("starting process\n");
        recv(sock, message, sizeof(message), 0);
        trimEnding(message);
        printf("received: %s\n", message);
        pthread_cleanup_push(errorRoutine, &sock);
        pthread_join(gopher(message, sock), NULL);
        pthread_cleanup_pop(0);
        printf("Exiting process...\n");
        exit(0);
    }
}

/*********************************************** LOGGER ***************************************************************/

pthread_mutex_t *mutexShare;
pthread_cond_t *condShare;

void logTransfer(char *log) {
    pthread_mutex_lock(mutexShare);
    write(logPipe, log, strlen(log));
    pthread_cond_signal(condShare);
    pthread_mutex_unlock(mutexShare);
}

/* Avvia il processo di logging dei trasferimenti. */
void startTransferLog() {
    int pid;
    int pipeFd[2];
    pthread_mutex_t mutex;
    pthread_mutexattr_t mutexAttr;
    pthread_cond_t cond;
    pthread_condattr_t condAttr;
    // Inizializza mutex e condition variable per notificare il logger che sono pronti nuovi dati sulla pipe.
    pthread_mutexattr_init(&mutexAttr);
    pthread_mutexattr_setpshared(&mutexAttr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&mutex, &mutexAttr);
    pthread_condattr_init(&condAttr);
    pthread_condattr_setpshared(&condAttr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&cond, &condAttr);
    mutexShare = mmap(NULL, sizeof(pthread_mutex_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (mutexShare == MAP_FAILED) {
        _err("startTransferLog() - impossibile mappare il mutex in memoria\n", ERR, true, -1);
    }
    *mutexShare = mutex;
    condShare = mmap(NULL, sizeof(pthread_cond_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (condShare == MAP_FAILED) {
        _err("startTransferLog() - impossibile mappare la condition variable in memoria\n", ERR, true, -1);
    }
    *condShare = cond;
    if (pipe2(pipeFd, O_DIRECT) < 0) {
        _err("startTransferLog() - impossibile aprire la pipe\n", ERR, true, -1);
    }
    if ((pid = fork()) < 0) {
        _err("startTransferLog() - fork fallita\n", ERR, true, pid);
    } else if (pid == 0) {  // Logger
        int logFile;
        char buff[PIPE_BUF + 1];
        loggerId = getpid();
        prctl(PR_SET_NAME, "Gopher logger");
        close(pipeFd[1]);
        logPipe = pipeFd[0];
        pthread_mutex_lock(mutexShare);
        while (1) {
            size_t bytesRead;
            pthread_cond_wait(condShare, mutexShare);
            if (requestShutdown) {
                pthread_mutex_unlock(mutexShare);
                break;
            }
            if ((logFile = open("logFile", O_WRONLY | O_CREAT | O_APPEND, S_IRWXU)) < 0) {
                _err("startTransferLog() - impossibile aprire il file di logging\n", ERR, true, pid);
                exit(-1);
            }
            if ((bytesRead = read(logPipe, buff, PIPE_BUF)) < 0) {
                close(logFile);
                exit(1);
            } else {
                write(logFile, buff, bytesRead);
            }
            close(logFile);
        }
        munmap(mutexShare, sizeof(pthread_mutex_t));
        munmap(condShare, sizeof(pthread_cond_t));
        free(mutexShare);
        free(condShare);
        close(logPipe);
        close(logFile);
    } else {  // Server
        close(pipeFd[0]);
        logPipe = pipeFd[1];  // logPipe è globale, viene acceduta per inviare i dati al logger
        loggerId = pid;
    }
}

#endif

/*****************************************************************************************************************/
/*                                             COMMON FUNCTIONS                                                 */

/*****************************************************************************************************************/

void _err(_cstring message, _cstring level, bool stderror, int code) {
    char error[50] = "";
    if (stderror) {
        errorString(error, 50);
    }
    printf("%s: %s - %s\n.", level, message, error);
    exit(code);
}

void initConfig(struct config *options) {
    options->port = DEFAULT_PORT;
    options->multiProcess = DEFAULT_MULTI_PROCESS;
}

bool readConfig(const _string configPath, struct config *options) {
    FILE *configFile;
    char port[6];
    char multiProcess[2];
    configFile = fopen(configPath, "r");
    if (configFile == NULL) {
        return errno;
    }
    while (fgetc(configFile) != '=')
        ;
    fgets(port, 6, configFile);
    while (fgetc(configFile) != '=')
        ;
    fgets(multiProcess, 2, configFile);
    fclose(configFile);
    options->port = atoi(port);
    options->multiProcess = (bool)atoi(multiProcess);
    return 0;
}