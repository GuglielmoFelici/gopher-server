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
int _shutdown() {
    closeSocket(server);
    AttachConsole(loggerId);
    GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, loggerId);
    printf("Shutting down...\n");
    exit(0);
}

void changeCwd(LPCSTR path) {
    SetCurrentDirectory(path);
}

/********************************************** SOCKETS *************************************************************/

/* Inizializza la WSA */
int startup() {
    WORD versionWanted = MAKEWORD(1, 1);
    WSADATA wsaData;
    if (!GetCurrentDirectory(sizeof(installationDir), installationDir)) {
        _err("Cannot get current working directory", ERR, true, -1);
    }
    _onexit(_shutdown);
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
void wakeUpServer() {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    sendto(s, "Wake up!", 0, 0, (struct sockaddr *)&awakeAddr, sizeof(awakeAddr));
    closeSocket(s);
}

/* Termina graziosamente il programma. */
BOOL ctrlC(DWORD signum) {
    if (signum == CTRL_C_EVENT) {
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
    sock = *(SOCKET *)args;
    free(args);
    recv(sock, message, sizeof(message), 0);
    trimEnding(message);
    printf("Request: %s\n", message);
    gopher(message, sock);  // Il protocollo viene eseguito qui
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
    char cmdLine[sizeof(exec) + 1 + sizeof(SOCKET)];  // TODO size??
    memset(&startupInfo, 0, sizeof(startupInfo));
    memset(&processInfo, 0, sizeof(processInfo));
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startupInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startupInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    snprintf(cmdLine, sizeof(cmdLine), "%s %d", exec, client);
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
    char exec[MAX_NAME];
    snprintf(exec, sizeof(exec), "%s/helpers/winLogger.exe", installationDir);
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
    if (!CreateProcess(exec, NULL, NULL, NULL, TRUE, 0, NULL, installationDir, &startupInfo, &processInfo)) {
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

int _shutdown() {
    close(logPipe);
    close(server);
    pthread_exit(0);
}

void errorString(char *error, size_t size) {
    snprintf(error, size, "%s", strerror(errno));
}

void changeCwd(const char *path) {
    chdir(path);
}

/* Rende il server un processo demone */
int startup() {
    int pid;
    getcwd(installationDir, sizeof(installationDir));
    atexit((void (*)(void))_shutdown);

    // pid = fork();
    // if (pid < 0) {
    //     _err(_DAEMON_ERR, ERR, true, errno);
    // } else if (pid > 0) {
    //     exit(0);
    // } else {
    //     sigset_t set;
    //     if (setsid() < 0) {
    //         _err(_DAEMON_ERR, ERR, true, errno);
    //     }
    //     sigemptyset(&set);
    //     sigaddset(&set, SIGHUP);
    //     sigprocmask(SIG_BLOCK, &set, NULL);
    //     pid = fork();
    //     if (pid < 0) {
    //         _err(_DAEMON_ERR, ERR, true, errno);
    //     } else if (pid > 0) {
    //         exit(0);
    //     } else {
    //         int devNull;
    //         devNull = open("/dev/null", O_RDWR);
    //         if (dup2(devNull, STDIN_FILENO) < 0 || dup2(devNull, STDOUT_FILENO) < 0 || dup2(devNull, STDERR_FILENO) < 0) {
    //             _err(_DAEMON_ERR, ERR, true, -1);
    //         }
    //         return close(devNull);
    //     }
    // }
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
void sigHandler(int signum) {
    updateConfig = true;
}

/* Richiede la terminazione */
void intHandler(int signum) {
    requestShutdown = true;
    if (getpid() == serverPid) {
        printf("Shutting down...\n");
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
    sHup.sa_flags = SA_NODEFER;
    sInt.sa_flags = 0;
    sigaction(SIGHUP, &sHup, NULL);
    sigaction(SIGINT, &sInt, NULL);
}

/*********************************************** THREADS & PROCESSES ***************************************************************/

void closeThread() {
    pthread_exit(NULL);
}

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
        _err(_FORK_ERR, ERR, true, errno);
    } else if (pid == 0) {
        runGopher(sock, true);
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

/* Logger */
void loggerLoop(int pipe) {
    int logFile, exitCode = 0;
    char buff[PIPE_BUF + 1];
    char logFilePath[MAX_NAME];
    prctl(PR_SET_NAME, "Gopher logger");
    snprintf(logFilePath, sizeof(logFilePath), "%s/logFile", installationDir);
    pthread_mutex_lock(mutexShare);
    while (1) {
        size_t bytesRead;
        pthread_cond_wait(condShare, mutexShare);
        if (requestShutdown) {
            pthread_mutex_unlock(mutexShare);
            break;
        }
        if ((logFile = open(logFilePath, O_WRONLY | O_CREAT | O_APPEND, S_IRWXU)) < 0) {
            printf(WARN " - Impossibile aprire il file di logging\n");
        }
        if ((bytesRead = read(pipe, buff, PIPE_BUF)) < 0) {
            exitCode = 1;
            break;
        } else {
            write(logFile, buff, bytesRead);
            close(logFile);
        }
    }
    munmap(mutexShare, sizeof(pthread_mutex_t));
    munmap(condShare, sizeof(pthread_cond_t));
    close(pipe);
    close(logFile);
    exit(0);
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
        loggerPid = getpid();
        close(pipeFd[1]);
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

void _err(_cstring message, _cstring level, bool stderror, int code) {
    char error[50] = "";
    if (stderror) {
        errorString(error, sizeof(error));
    }
    printf("%s: %s - %s\n", level, message, error);
    exit(code);
}

void defaultConfig(struct config *options, int which) {
    if (which == READ_PORT || which == READ_BOTH) {
        options->port = DEFAULT_PORT;
    }
    if (which == READ_MULTIPROCESS || which == READ_BOTH) {
        options->multiProcess = DEFAULT_MULTI_PROCESS;
    }
}

int readConfig(struct config *options, int which) {
    char configPath[MAX_NAME];
    FILE *configFile;
    char port[6];
    char multiProcess[2];
    snprintf(configPath, sizeof(configPath), "%s/%s", installationDir, CONFIG_FILE);
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
    if (which == READ_PORT || which == READ_BOTH) {
        options->port = atoi(port);
    }
    if (which == READ_MULTIPROCESS || which == READ_BOTH) {
        options->multiProcess = (bool)atoi(multiProcess);
    }
    return 0;
}