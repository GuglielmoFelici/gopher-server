#include "headers/gopher.h"

#if defined(_WIN32)

/*****************************************************************************************************************/
/*                                             WINDOWS FUNCTIONS                                                 */

/*****************************************************************************************************************/

/************************************************** UTILS ********************************************************/

void errorString(char *error, size_t size) {
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  error, size, NULL);
}

/* Termina graziosamente il logger, poi termina il server. */
void _shutdown() {
    closesocket(server);
    closesocket(awakeSelect);
    CloseHandle(logEvent);
    CloseHandle(logPipe);
    printf("Shutting down...\n");
    // FreeConsole();
    // if (AttachConsole(loggerPid)) {
    //     GenerateConsoleCtrlEvent(CTRL_C_EVENT, loggerPid);
    // }
}

void changeCwd(LPCSTR path) {
    if (!SetCurrentDirectory(path)) {
        _logErr(WARN " - Can't change current working directory");
    }
}

/********************************************** SOCKETS *************************************************************/

/* Inizializzazione di console e WSA */
int startup() {
    WSADATA wsaData;
    WORD versionWanted = MAKEWORD(1, 1);
    if (!GetCurrentDirectory(sizeof(installationDir), installationDir)) {
        _err("Cannot get current working directory", true, -1);
    }
    return WSAStartup(versionWanted, &wsaData);
}

/* Ritorna l'ultimo codice di errore relativo alle chiamate WSA */
int sockErr() {
    return WSAGetLastError();
}

int closeSocket(SOCKET s) {
    return closesocket(s);
}

/********************************************** SIGNALS *************************************************************/

/* Invia un messaggio vuoto al socket awakeSelect per interrompere la select del server. */
int wakeUpServer() {
    SOCKET s;
    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) {
        return -1;
    }
    return sendto(s, "Wake up!", 0, 0, (struct sockaddr *)&awakeAddr, sizeof(awakeAddr));
}

/* Termina graziosamente il programma. */
BOOL ctrlC(DWORD signum) {
    if (signum == CTRL_C_EVENT) {
        requestShutdown = true;
        if (wakeUpServer() < 0) {
            _logErr("Can't close gracefully");
            exit(-1);
        }
        return true;
    }
    return false;
}

/* Richiede la rilettura del file di configurazione */
BOOL sigHandler(DWORD signum) {
    if (signum != CTRL_C_EVENT) {
        updateConfig = true;
        if (wakeUpServer() < 0) {
            _logErr("Error updating");
        }
        return true;
    }
    return false;
}

/* Installa i gestori di eventi */
void installDefaultSigHandlers() {
    awakeSelect = socket(AF_INET, SOCK_DGRAM, 0);
    if (awakeSelect == INVALID_SOCKET) {
        _err("installSigHandlers() - Error creating awake socket", true, -1);
    }
    memset(&awakeAddr, 0, sizeof(awakeAddr));
    awakeAddr.sin_family = AF_INET;
    awakeAddr.sin_port = 0;
    awakeAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int awakeAddrSize = sizeof(awakeAddr);
    if (bind(awakeSelect, (struct sockaddr *)&awakeAddr, sizeof(awakeAddr)) == SOCKET_ERROR) {
        _err("installSigHandlers() - Error binding awake socket", true, -1);
    }
    if (getsockname(awakeSelect, (struct sockaddr *)&awakeAddr, &awakeAddrSize) == SOCKET_ERROR) {
        _err("installSigHandlers() - Can't detect socket address info", true, -1);
    }
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)ctrlC, TRUE);
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)sigHandler, TRUE);
}

/*********************************************** THREADS & PROCESSES ***************************************************************/

void closeThread() {
    ExitThread(0);
}

/* Task lanciato dal server per avviare un thread che esegue il protocollo Gopher. */
void *serveThreadTask(void *args) {
    SOCKET sock;
    sock = *(SOCKET *)args;
    free(args);
    gopher(sock);  // Il protocollo viene eseguito qui
}

/* Serve una richiesta in modalità multithreading. */
void serveThread(SOCKET *sock) {
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)serveThreadTask, sock, 0, NULL);
}

/* Serve una richiesta in modalità multiprocesso. */
void serveProc(SOCKET client) {
    char exec[MAX_NAME];
    snprintf(exec, sizeof(exec), "%s/helpers/winGopherProcess.exe", installationDir);
    STARTUPINFO startupInfo;
    PROCESS_INFORMATION processInfo;
    int cmdLineSize = 1;
    LPSTR cmdLine = calloc(1, 1);
    memset(&startupInfo, 0, sizeof(startupInfo));
    memset(&processInfo, 0, sizeof(processInfo));
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startupInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startupInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    do {
        cmdLineSize += 30;
        cmdLine = realloc(cmdLine, cmdLineSize);
        if (cmdLine == NULL) {
            _logErr("Error serving the client with a new process");
            return;
        }
    } while (snprintf(cmdLine, sizeof(cmdLine), "%s %d %p %p", exec, client, logPipe, logEvent));
    CreateProcess(exec, cmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &startupInfo, &processInfo);
    free(cmdLine);
}

/*********************************************** LOGGER ***************************************************************/

void logTransfer(LPSTR log) {
    // TODO mutex
    DWORD written;
    if (!WriteFile(logPipe, log, strlen(log), &written, NULL)) {
        return;
    }
    SetEvent(logEvent);
}

/* Avvia il processo di logging dei trasferimenti. */
void startTransferLog() {
    char exec[MAX_NAME];
    HANDLE readPipe;
    SECURITY_ATTRIBUTES attr;
    STARTUPINFO startupInfo;
    PROCESS_INFORMATION processInfo;
    snprintf(exec, sizeof(exec), "%s/helpers/winLogger.exe", installationDir);
    memset(&attr, 0, sizeof(attr));
    attr.bInheritHandle = TRUE;
    attr.nLength = sizeof(attr);
    attr.lpSecurityDescriptor = NULL;
    // logPipe è globale/condivisa, viene acceduta in scrittura quando avviene un trasferimento file
    if (!CreatePipe(&readPipe, &logPipe, &attr, 0)) {
        _logErr("startTransferLog() - Can't create pipe");
        return;
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
        _logErr("startTransferLog() - Can't create logEvent");
    } else if (!CreateProcess(exec, NULL, NULL, NULL, TRUE, 0, NULL, installationDir, &startupInfo, &processInfo)) {
        _logErr("startTransferLog() - Starting logger failed");
    } else {
        loggerPid = processInfo.dwProcessId;
    }
    CloseHandle(readPipe);
}

#else

/*****************************************************************************************************************/
/*                                             UNIX FUNCTIONS                                                    */

/*****************************************************************************************************************/

/************************************************** UTILS ********************************************************/

/* Termina il logger ed esce. */
void _shutdown() {
    kill(loggerPid, SIGINT);
    close(logPipe);
    close(server);
    pthread_exit(0);
}

void errorString(char *error, size_t size) {
    snprintf(error, size, "%s", strerror(errno));
}

void changeCwd(const char *path) {
    if (chdir(path) < 0) {
        _logErr(WARN " - Can't change current working directory");
    }
}

/* Rende il server un processo demone */
int startup() {
    int pid;
    serverPid = getpid();
    getcwd(installationDir, sizeof(installationDir));

    pid = fork();
    if (pid < 0) {
        _err(_DAEMON_ERR, true, errno);
    } else if (pid > 0) {
        exit(0);
    } else {
        sigset_t set;
        if (setsid() < 0) {
            _err(_DAEMON_ERR, true, errno);
        }
        sigemptyset(&set);
        sigaddset(&set, SIGHUP);
        sigprocmask(SIG_BLOCK, &set, NULL);
        pid = fork();
        if (pid < 0) {
            _err(_DAEMON_ERR, true, errno);
        } else if (pid > 0) {
            exit(0);
        } else {
            int serverStdIn, serverStdErr, serverStdOut;
            char fileName[PATH_MAX];
            sigprocmask(SIG_UNBLOCK, &set, NULL);
            serverStdIn = open("/dev/null", O_RDWR);
            snprintf(fileName, PATH_MAX, "%s/serverStdOut", installationDir);
            serverStdOut = creat(fileName, S_IRWXU);
            snprintf(fileName, PATH_MAX, "%s/serverStdErr", installationDir);
            serverStdErr = creat(fileName, S_IRWXU);
            if (dup2(serverStdIn, STDIN_FILENO) < 0 || dup2(serverStdOut, STDOUT_FILENO) < 0 || dup2(serverStdErr, STDERR_FILENO) < 0) {
                _err(_DAEMON_ERR, true, -1);
            }
            return close(serverStdIn) + close(serverStdOut) + close(serverStdErr);
        }
    }
    return 0;
}

/********************************************** SOCKETS *************************************************************/

int sockErr() {
    return errno;
}

int closeSocket(int s) {
    return close(s);
}

/********************************************** SIGNALS *************************************************************/

/* Richiede la rilettura del file di configurazione */
void hupHandler(int signum) {
    printf("laido\n");
    updateConfig = true;
}

/* Richiede la terminazione */
void intHandler(int signum) {
    printf("Shutting down...\n");
    requestShutdown = true;
}

/* Installa un gestore di segnale */
void installSigHandler(int sig, void (*func)(int), int flags) {
    struct sigaction sigact;
    sigact.sa_handler = func;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = flags;
    sigaction(sig, &sigact, NULL);
}

/* Installa i gestori predefiniti di segnali */
void installDefaultSigHandlers() {
    installSigHandler(SIGINT, &intHandler, 0);
    installSigHandler(SIGHUP, &hupHandler, SA_NODEFER);
}

/*********************************************** THREADS & PROCESSES ***************************************************************/

void closeThread() {
    pthread_exit(NULL);
}

/* Avvia il processo gopher, predisponendo la routine di cleanup errorRoutine */
void runGopher(int sock, bool multiProcess) {
    pthread_cleanup_push(errorRoutine, &sock);
    _thread tid = gopher(sock);
    if (multiProcess) {
        pthread_join(tid, NULL);
    } else {
        pthread_detach(tid);
    }
    pthread_cleanup_pop(0);
}

/* Task lanciato dal server per avviare un thread che esegue il protocollo Gopher. */
void *serveThreadTask(void *args) {
    sigset_t set;
    char message[MAX_GOPHER_MSG];
    int sock;
    sigemptyset(&set);
    sigaddset(&set, SIGHUP);
    pthread_sigmask(SIG_BLOCK, &set, NULL);
    sock = *(int *)args;
    free(args);
    runGopher(sock, false);
}

/* Serve una richiesta in modalità multithreading. */
void serveThread(int *sock) {
    pthread_t tid;
    if (pthread_create(&tid, NULL, serveThreadTask, sock)) {
        printf(_THREAD_ERR "\n");
        return;
    }
    pthread_detach(tid);
}

/* Serve una richiesta in modalità multiprocesso. */
void serveProc(int sock) {
    pid_t pid;
    pid = fork();
    if (pid < 0) {
        _err(_FORK_ERR, true, errno);
    } else if (pid == 0) {
        runGopher(sock, true);
        exit(0);
    }
}

/*********************************************** LOGGER ***************************************************************/

pthread_mutex_t *mutexShare;
pthread_cond_t *condShare;

/* Effettua una scrittura sulla pipe di logging */
void logTransfer(char *log) {
    pthread_mutex_lock(mutexShare);
    write(logPipe, log, strlen(log));
    pthread_cond_signal(condShare);
    pthread_mutex_unlock(mutexShare);
}

/* Handler per SIGINT relativo al logger */
void logIntHandler(int signum) {
    requestShutdown = true;
    pthread_cond_signal(condShare);
}

/* Loop di logging */
void loggerLoop(int inPipe) {
    int logFile, exitCode = 0;
    char buff[PIPE_BUF];
    char logFilePath[MAX_NAME];
    prctl(PR_SET_NAME, "Gopher logger");
    snprintf(logFilePath, sizeof(logFilePath), "%s/logFile", installationDir);
    if ((logFile = open(logFilePath, O_WRONLY | O_CREAT | O_APPEND, S_IRWXU)) < 0) {
        printf(WARN " - Can't start logger\n");
    }
    pthread_mutex_lock(mutexShare);
    while (1) {
        size_t bytesRead;
        if (requestShutdown) {
            pthread_mutex_unlock(mutexShare);
            break;
        }
        pthread_cond_wait(condShare, mutexShare);
        if ((bytesRead = read(inPipe, buff, PIPE_BUF)) < 0) {
            exitCode = 1;
            break;
        } else {
            write(logFile, buff, bytesRead);
        }
    }
    munmap(mutexShare, sizeof(pthread_mutex_t));
    munmap(condShare, sizeof(pthread_cond_t));
    close(inPipe);
    close(logFile);
    exit(exitCode);
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
        _err("startTransferLog() - impossibile mappare il mutex in memoria\n", true, -1);
    }
    *mutexShare = mutex;
    condShare = mmap(NULL, sizeof(pthread_cond_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (condShare == MAP_FAILED) {
        _err("startTransferLog() - impossibile mappare la condition variable in memoria\n", true, -1);
    }
    *condShare = cond;
    if (pipe2(pipeFd, O_DIRECT) < 0) {
        _err("startTransferLog() - impossibile aprire la pipe\n", true, -1);
    }
    if ((pid = fork()) < 0) {
        _err("startTransferLog() - fork fallita\n", true, pid);
    } else if (pid == 0) {  // Logger
        loggerPid = getpid();
        close(pipeFd[1]);
        installSigHandler(SIGINT, logIntHandler, 0);
        loggerLoop(pipeFd[0]);
    } else {  // Server
        close(pipeFd[0]);
        logPipe = pipeFd[1];  // logPipe è globale, viene acceduta per inviare i dati al logger
        loggerPid = pid;
    }
}

#endif

/*****************************************************************************************************************/
/*                                             COMMON FUNCTIONS                                                 */

/*****************************************************************************************************************/

/* Stampa l'errore su stderr ed esce */
void _err(_cstring message, bool stderror, int code) {
    char error[MAX_ERROR_SIZE] = "";
    if (stderror) {
        errorString(error, sizeof(error));
    }
    fprintf(stderr, "%s - %s\n", message, error);
    exit(code);
}

/* Stampa l'errore su stderr */
void _logErr(_cstring message) {
    char buf[MAX_ERROR_SIZE];
    errorString(buf, MAX_ERROR_SIZE);
    fprintf(stderr, "%s\nSystem error message: %s", message, buf);
}

void defaultConfig(struct config *options, int which) {
    if (which & READ_PORT) {
        options->port = DEFAULT_PORT;
    }
    if (which & READ_MULTIPROCESS) {
        options->multiProcess = DEFAULT_MULTI_PROCESS;
    }
}

int readConfig(struct config *options, int which) {
    char *configPath, *endptr;
    FILE *configFile;
    char port[6], multiProcess[2];
    size_t configPathSize = strlen(installationDir) + strlen(CONFIG_FILE) + 2;
    if ((configPath = malloc(configPathSize)) == NULL) {
        return -1;
    }
    if (snprintf(configPath, configPathSize, "%s/%s", installationDir, CONFIG_FILE) < configPathSize - 1) {
        return -1;
    }
    if ((configFile = fopen(configPath, "r")) == NULL) {
        return -1;
    }
    free(configPath);
    while (fgetc(configFile) != '=')
        ;
    fgets(port, sizeof(port), configFile);
    while (fgetc(configFile) != '=')
        ;
    fgets(multiProcess, sizeof(multiProcess), configFile);
    if (fclose(configFile) != 0) {
        return -1;
    }
    if (which & READ_PORT) {
        options->port = strtol(port, &endptr, 10);
        if (options->port < 1 || options->port > 65535) {
            return -1;
        }
    }
    if (which & READ_MULTIPROCESS) {
        options->multiProcess = strtol(multiProcess, &endptr, 10);
    }
    return 0;
}