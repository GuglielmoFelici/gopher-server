#include "../headers/server.h"
#include <stdbool.h>
#include <stdio.h>
#include "../headers/datatypes.h"

static _sig_atomic volatile updateConfig = false;     // Controllo della modifica del file di configurazione
static _sig_atomic volatile requestShutdown = false;  // Chiusura dell'applicazione
static socket_t awakeSelect;                          // Socket per interrompere la select su windows
static struct sockaddr_in awakeAddr;

#if defined(_WIN32)

/*****************************************************************************************************************/
/*                                             WINDOWS FUNCTIONS                                                 */

/*****************************************************************************************************************/

static void printHeading(const server_t* pServer) {
    printf("Listening on port %d (%s mode)\n", pServer->port, pServer->multiProcess ? "multiprocess" : "multithreaded");
}

/* Inizializzazione di console e WSA */
int startup() {
    WSADATA wsaData;
    WORD versionWanted = MAKEWORD(1, 1);
    return WSAStartup(versionWanted, &wsaData) == 0 ? SERVER_SUCCESS : SERVER_FAILURE;
}

/* Invia un messaggio vuoto al socket awakeSelect per interrompere la select del server. */
static int wakeUpServer() {
    SOCKET s;
    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) {
        return -1;
    }
    return sendto(s, "Wake up!", 0, 0, (struct sockaddr*)&awakeAddr, sizeof(awakeAddr));
}

/********************************************** SIGNALS *************************************************************/

/* Termina graziosamente il programma. */
static BOOL ctrlC(DWORD signum) {
    if (signum == CTRL_C_EVENT) {
        requestShutdown = true;
        if (wakeUpServer() < 0) {
            _logErr("Can't close gracefully");
            exit(1);
        }
        return true;
    }
    return false;
}

/* Richiede la rilettura del file di configurazione */
static BOOL sigHandler(DWORD signum) {
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
int installDefaultSigHandlers() {
    awakeSelect = socket(AF_INET, SOCK_DGRAM, 0);
    if (INVALID_SOCKET == awakeSelect) {
        return SERVER_FAILURE;
    }
    memset(&awakeAddr, 0, sizeof(awakeAddr));
    awakeAddr.sin_family = AF_INET;
    awakeAddr.sin_port = 0;
    awakeAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    size_t awakeAddrSize = sizeof(awakeAddr);
    if (SOCKET_ERROR == bind(awakeSelect, (struct sockaddr*)&awakeAddr, sizeof(awakeAddr))) {
        return SERVER_FAILURE;
    }
    if (SOCKET_ERROR == getsockname(awakeSelect, (struct sockaddr*)&awakeAddr, &awakeAddrSize)) {
        return SERVER_FAILURE;
    }
    if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)ctrlC, TRUE)) {
        return SERVER_FAILURE;
    }
    if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)sigHandler, TRUE)) {
        return SERVER_FAILURE;
    }
}

/*********************************************** THREADS & PROCESSES ***************************************************************/

/* Task lanciato dal server per avviare un thread che esegue il protocollo Gopher. */
static DWORD WINAPI serveThreadTask(LPVOID args) {
    struct threadArgs gopherArgs = *(struct threadArgs*)args;
    free(args);
    gopher(gopherArgs.sock, gopherArgs.port);  // Il protocollo viene eseguito qui TODO waitForSend false è giusto??
}

/* Serve una richiesta in modalità multithreading. */
static int serveThread(SOCKET sock, unsigned short port) {
    HANDLE thread;
    struct threadArgs* args;
    if ((args = malloc(sizeof(struct threadArgs))) == NULL) {
        return SERVER_FAILURE;
    }
    args->sock = sock;
    args->port = port;
    if ((thread = CreateThread(NULL, 0, serveThreadTask, args, 0, NULL)) == NULL) {
        return SERVER_FAILURE;
    }
    CloseHandle(thread);
    return SERVER_SUCCESS;
}

/* Serve una richiesta in modalità multiprocesso. */
static int serveProc(SOCKET client, unsigned short port) {
    STARTUPINFO startupInfo;
    PROCESS_INFORMATION processInfo;
    char exec[MAX_PATH];
    LPSTR cmdLine;
    size_t cmdLineSize;
    if (snprintf(exec, sizeof(exec), "%s/" HELPER_PATH, installationDir) < strlen(installationDir) + strlen(HELPER_PATH) + 1) {
        return SERVER_FAILURE;
    }
    memset(&startupInfo, 0, sizeof(startupInfo));
    memset(&processInfo, 0, sizeof(processInfo));
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    if (
        (startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE)) == INVALID_HANDLE_VALUE ||
        (startupInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE)) == INVALID_HANDLE_VALUE ||
        (startupInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE)) == INVALID_HANDLE_VALUE) {
        return SERVER_FAILURE;
    }
    cmdLineSize = snprintf(NULL, 0, "%s %hu %p %p %p", exec, port, client, logPipe, logEvent) + 1;
    if ((cmdLine = malloc(cmdLineSize)) == NULL) {
        return SERVER_FAILURE;
    }
    if (
        snprintf(cmdLine, cmdLineSize, "%s %hu %p %p %p", exec, port, client, logPipe, logEvent) < cmdLineSize - 1 ||
        !CreateProcess(exec, cmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &startupInfo, &processInfo)) {
        free(cmdLine);
        return SERVER_FAILURE;
    }
    return SERVER_SUCCESS;
}

#else

/*****************************************************************************************************************/
/*                                             LINUX FUNCTIONS                                                    */

/*****************************************************************************************************************/

static void printHeading(struct config* options) {
    printf("Started daemon with pid %d\n", getpid());
    printf("Listening on port %i (%s mode)\n", options->port, options->multiProcess ? "multiprocess" : "multithreaded");
}

/* Rende il server un processo demone */
int startup() {
    int pid;
    // pid = fork();
    pid = 0;
    if (pid < 0) {
        return SERVER_FAILURE;
    } else if (pid > 0) {
        exit(0);
    } else {
        sigset_t set;
        // if (setsid() < 0) {
        //     _err(_DAEMON_ERR, true, errno);
        // }
        if (sigemptyset(&set) < 0) {
            return SERVER_FAILURE;
        }
        if (sigaddset(&set, SIGHUP) < 0) {
            return SERVER_FAILURE;
        }
        if (sigprocmask(SIG_BLOCK, &set, NULL) < 0) {
            return SERVER_FAILURE;
        }
        // pid = fork();
        pid = 0;
        if (pid < 0) {
            return SERVER_FAILURE;
        } else if (pid > 0) {
            exit(0);
        } else {
            int serverStdIn, serverStdErr, serverStdOut;
            char fileName[PATH_MAX];
            if (sigprocmask(SIG_UNBLOCK, &set, NULL) < 0) {
                return SERVER_FAILURE;
            }
            // TODO controllare che cazzo fare con gli stream
            // TODO eventualmente usare syslog
            if ((serverStdIn = open("/dev/null", O_RDWR)) < 0) {
                return SERVER_FAILURE;
            }
            snprintf(fileName, PATH_MAX, "%s/serverStdOut", installationDir);
            serverStdOut = creat(fileName, S_IRWXU);
            snprintf(fileName, PATH_MAX, "%s/serverStdErr", installationDir);
            serverStdErr = creat(fileName, S_IRWXU);
            if (dup2(serverStdIn, STDIN_FILENO) < 0)
                ;  //|| dup2(serverStdOut, STDOUT_FILENO) < 0 || dup2(serverStdErr, STDERR_FILENO) < 0) {
            //     _err(_DAEMON_ERR, true, -1);
            // }
            return close(serverStdIn) + close(serverStdOut) + close(serverStdErr);
        }
    }
    return 0;
}

/********************************************** SIGNALS *************************************************************/

/* Richiede la rilettura del file di configurazione */
static void hupHandler(int signum) {
    updateConfig = true;
}

/* Richiede la terminazione */
static void intHandler(int signum) {
    printf("Shutting down...\n");
    requestShutdown = true;
}

/* Installa un gestore di segnale */
static int installSigHandler(int sig, void (*func)(int), int flags) {
    struct sigaction sigact;
    sigact.sa_handler = func;
    sigact.sa_flags = flags;
    if (sigemptyset(&sigact.sa_mask) != 0 ||
        sigaction(sig, &sigact, NULL) != 0) {
        return SERVER_FAILURE;
    }
    return SERVER_SUCCESS;
}

/* Installa i gestori predefiniti di segnali */
int installDefaultSigHandlers() {
    if (installSigHandler(SIGINT, &intHandler, 0) != SERVER_SUCCESS ||
        installSigHandler(SIGHUP, &hupHandler, SA_NODEFER) != SERVER_SUCCESS) {
        _err(_SYS_ERR, true, errno);
    }
}

/*********************************************** THREADS & PROCESSES ***************************************************************/

/* Task lanciato dal server per avviare un thread che esegue il protocollo Gopher. */
static void* serveThreadTask(void* args) {
    sigset_t set;
    int sock;
    struct threadArgs tArgs;
    if (
        sigemptyset(&set) < 0 ||
        sigaddset(&set, SIGHUP) < 0 ||
        pthread_sigmask(SIG_BLOCK, &set, NULL) != 0) {
        free(args);
        exit(1);
    }
    tArgs = *(struct threadArgs*)args;
    free(args);
    gopher(tArgs.sock, tArgs.port);
}

/* Serve una richiesta in modalità multithreading. */
static int serveThread(int sock, unsigned short port) {
    pthread_t tid;
    struct threadArgs* tArgs;
    if ((tArgs = malloc(sizeof(struct threadArgs))) == NULL) {
        return SERVER_FAILURE;
    }
    tArgs->sock = sock;
    tArgs->port = port;
    if (pthread_create(&tid, NULL, serveThreadTask, tArgs)) {
        // TODO error
        printf(_THREAD_ERR "\n");
        return SERVER_FAILURE;
    }
    pthread_detach(tid);
    return SERVER_SUCCESS;
}

/* Serve una richiesta in modalità multiprocesso. */
static int serveProc(int sock, unsigned short port) {
    pid_t pid;
    pid = fork();
    if (pid < 0) {
        return SERVER_FAILURE;
    } else if (pid == 0) {
        gopher(sock, port);
        pthread_exit(0);
    } else {
        return SERVER_SUCCESS;
    }
}

#endif

/*****************************************************************************************************************/
/*                                             COMMON FUNCTIONS                                                 */

/*****************************************************************************************************************/

/* Inizializza il socket del server e lo mette in ascolto */
int prepareSocket(server_t* pServer, int flags) {
    if (flags & SERVER_UPDATE) {
        if (closeSocket(pServer->sock) < 0) {
            return SERVER_FAILURE;
        };
    }
    if (INVALID_SOCKET == (pServer->sock = socket(AF_INET, SOCK_STREAM, 0))) {
        return SERVER_FAILURE;
    }
    pServer->sockAddr.sin_family = AF_INET;
    pServer->sockAddr.sin_addr.s_addr = INADDR_ANY;
    pServer->sockAddr.sin_port = htons(pServer->port);
    if (bind(pServer->sock, (struct sockaddr*)&(pServer->sockAddr), sizeof(pServer->sockAddr)) < 0) {
        return SERVER_FAILURE;
    }
    if (listen(pServer->sock, 5) < 0) {
        return SERVER_FAILURE;
    }
    return SERVER_SUCCESS;
}

void defaultConfig(server_t* pServer, int which) {
    if (which & READ_PORT) {
        pServer->port = DEFAULT_PORT;
    }
    if (which & READ_MULTIPROCESS) {
        pServer->multiProcess = DEFAULT_MULTI_PROCESS;
    }
}

int readConfig(server_t* pServer, int which) {
    // TODO error check
    FILE* configFile = NULL;
    char* configPath = NULL;
    size_t configPathSize = strlen(pServer->installationDir) + strlen(CONFIG_FILE) + 2;
    if (NULL == (configPath = malloc(configPathSize))) {
        goto ON_ERROR;
    }
    if (snprintf(configPath, configPathSize, "%s/%s", pServer->installationDir, CONFIG_FILE) < configPathSize - 1) {
        goto ON_ERROR;
    }
    if (NULL == (configFile = fopen(configPath, "r"))) {
        goto ON_ERROR;
    }
    free(configPath);
    char portBuff[6], multiProcess[2];
    while (fgetc(configFile) != CONFIG_DELIMITER)
        ;
    fgets(portBuff, sizeof(portBuff), configFile);
    while (fgetc(configFile) != CONFIG_DELIMITER)
        ;
    fgets(multiProcess, sizeof(multiProcess), configFile);
    if (fclose(configFile) != 0) {
        goto ON_ERROR;
    }
    char* strtolptr = NULL;
    if (which & READ_PORT) {
        int port;
        port = strtol(port, &strtolptr, 10);
        if (port < 1 || port > 65535) {
            goto ON_ERROR;
        }
        pServer->port = port;
    }
    if (which & READ_MULTIPROCESS) {
        pServer->multiProcess = strtol(multiProcess, &strtolptr, 10);
    }
    return SERVER_SUCCESS;
ON_ERROR:
    if (configPath) {
        free(configPath);
        return SERVER_FAILURE;
    }
}

int startServer(server_t* pServer) {
    int ready = 0;
    while (true) {
        do {
            if (requestShutdown) {
                _shutdown(server);
            } else if (ready == SOCKET_ERROR && sockErr() != EINTR) {
                _err(_SELECT_ERR, true, -1);
            } else if (updateConfig) {
                printf("Updating config...\n");
                int prevMultiprocess = options.multiProcess;
                if (readConfig(&options, READ_PORT | READ_MULTIPROCESS) != 0) {
                    _logErr(WARN " - " _CONFIG_ERR);
                    defaultConfig(&options, READ_PORT);
                }
                if (options.port != htons(serverAddr.sin_port)) {
                    server = prepareServer(server, &options, &serverAddr);
                    printf("Switched to port %i\n", options.port);
                }
                if (options.multiProcess != prevMultiprocess) {
                    printf("Switched to %s mode\n", options.multiProcess ? "multiprocess" : "multithreaded");
                }
                updateConfig = false;
            }
            FD_ZERO(&incomingConnections);
            FD_SET(server, &incomingConnections);
            FD_SET(awakeSelect, &incomingConnections);
        } while ((ready = select(server + 1, &incomingConnections, NULL, NULL, NULL)) < 0 || !FD_ISSET(server, &incomingConnections));
        printf("Incoming connection on port %d\n", htons(serverAddr.sin_port));
        _socket client = accept(server, NULL, NULL);
        if (client == INVALID_SOCKET) {
            _logErr(WARN "Error serving client");
        } else if (options.multiProcess) {
            // TODO controllare return value
            serveProc(client, options.port);
            closeSocket(client);
        } else {
            serveThread(client, options.port);
        }
    }
}