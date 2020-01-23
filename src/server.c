#include "../headers/server.h"
#include <stdbool.h>
#include <stdio.h>
#include "../headers/datatypes.h"
#include "../headers/log.h"
#include "../headers/logger.h"
#include "../headers/platform.h"
#include "../headers/protocol.h"

static _sig_atomic volatile updateConfig = false;     // Controllo della modifica del file di configurazione
static _sig_atomic volatile requestShutdown = false;  // Chiusura dell'applicazione
static socket_t awakeSelect;                          // Socket per interrompere la select su windows
static struct sockaddr_in awakeAddr;

#if defined(_WIN32)

/*****************************************************************************************************************/
/*                                             WINDOWS FUNCTIONS                                                 */

/*****************************************************************************************************************/

#include <windows.h>

void printHeading(const server_t* pServer) {
    printf("Listening on port %d (%s mode)\n", pServer->port, pServer->multiProcess ? "multiprocess" : "multithreaded");
}

/* Inizializza la WSA */
int initServer(server_t* pServer) {
    strncpy(pServer->installationDir, "", sizeof(pServer->installationDir));
    pServer->sock = INVALID_SOCKET;
    memset(&(pServer->sockAddr), 0, sizeof(&(pServer->sockAddr)));
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
    return SERVER_SUCCESS;
}

/*********************************************** THREADS & PROCESSES ***************************************************************/

/* Task lanciato dal server per avviare un thread che esegue il protocollo Gopher. */
static DWORD WINAPI serveThreadTask(LPVOID args) {
    server_thread_args_t gopherArgs = *(server_thread_args_t*)args;
    free(args);
    gopher(gopherArgs.sock, gopherArgs.port, gopherArgs.pLogger);
}

/* Serve una richiesta in modalità multithreading. */
static int serveThread(SOCKET sock, int port, logger_t* pLogger) {
    HANDLE thread;
    server_thread_args_t* args;
    if (NULL == (args = malloc(sizeof(server_thread_args_t)))) {
        return SERVER_FAILURE;
    }
    args->sock = sock;
    args->port = port;
    args->pLogger = pLogger;
    if ((thread = CreateThread(NULL, 0, serveThreadTask, args, 0, NULL)) == NULL) {
        free(args);
        return SERVER_FAILURE;
    }
    CloseHandle(thread);
    return SERVER_SUCCESS;
}

/* Serve una richiesta in modalità multiprocesso. */
static int serveProc(SOCKET client, const logger_t* pLogger, const server_t* pServer) {
    char exec[MAX_NAME];
    if (snprintf(exec, sizeof(exec), "%s/" HELPER_PATH, pServer->installationDir) < strlen(pServer->installationDir) + strlen(HELPER_PATH) + 1) {
        return SERVER_FAILURE;
    }
    STARTUPINFO startupInfo;
    PROCESS_INFORMATION processInfo;
    memset(&startupInfo, 0, sizeof(startupInfo));
    memset(&processInfo, 0, sizeof(processInfo));
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    if (
        INVALID_HANDLE_VALUE == (startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE)) ||
        INVALID_HANDLE_VALUE == (startupInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE)) ||
        INVALID_HANDLE_VALUE == (startupInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE))) {
        return SERVER_FAILURE;
    }
    LPSTR cmdLine = NULL;
    size_t cmdLineSize;
    cmdLineSize = snprintf(NULL, 0, "%s %hu %p %p %p", exec, pServer->port, client, pLogger->logPipe, pLogger->logEvent) + 1;
    if (NULL == (cmdLine = malloc(cmdLineSize))) {
        return SERVER_FAILURE;
    }
    if (snprintf(cmdLine, cmdLineSize, "%s %hu %p %p %p", exec, pServer->port, client, pLogger->logPipe, pLogger->logEvent) < cmdLineSize - 1) {
        free(cmdLine);
        return SERVER_FAILURE;
    }
    if (!CreateProcess(exec, cmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &startupInfo, &processInfo)) {
        free(cmdLine);
        return SERVER_FAILURE;
    }
    free(cmdLine);
    return SERVER_SUCCESS;
}

#else

/*****************************************************************************************************************/
/*                                             LINUX FUNCTIONS                                                    */

/*****************************************************************************************************************/

void printHeading(const server_t* pServer) {
    printf("Started daemon with pid %d\n", getpid());
    printf("Listening on port %i (%s mode)\n", pServer->port, pServer->multiProcess ? "multiprocess" : "multithreaded");
}

int initServer(server_t* pServer) {
    strncpy(pServer->installationDir, "", sizeof(pServer->installationDir));
    pServer->sock = INVALID_SOCKET;
    memset(&(pServer->sockAddr), 0, sizeof(&(pServer->sockAddr)));
    return SERVER_SUCCESS;
}

/********************************************** SIGNALS *************************************************************/

/* Richiede la rilettura del file di configurazione */
static void hupHandler(int signum) {
    updateConfig = true;
}

/* Richiede la terminazione */
static void intHandler(int signum) {
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
    if (SERVER_SUCCESS != installSigHandler(SIGINT, &intHandler, 0)) {
        return SERVER_FAILURE;
    }
    return installSigHandler(SIGHUP, &hupHandler, SA_NODEFER);
}

/*********************************************** THREADS & PROCESSES ***************************************************************/

/* Task lanciato dal server per avviare un thread che esegue il protocollo Gopher. */
static void* serveThreadTask(void* args) {
    int sock;
    sigset_t set;
    server_thread_args_t tArgs = *(server_thread_args_t*)args;
    free(args);
    if (
        sigemptyset(&set) < 0 ||
        sigaddset(&set, SIGHUP) < 0 ||
        pthread_sigmask(SIG_BLOCK, &set, NULL) != 0) {
        exit(1);
    }
    gopher(tArgs.sock, tArgs.port, tArgs.pLogger);
}

/* Serve una richiesta in modalità multithreading. */
static int serveThread(int sock, int port, logger_t* pLogger) {
    server_thread_args_t* tArgs = malloc(sizeof(server_thread_args_t));
    if (NULL == tArgs) {
        return SERVER_FAILURE;
    }
    tArgs->sock = sock;
    tArgs->port = port;
    tArgs->pLogger = pLogger;
    pthread_t tid;
    if (pthread_create(&tid, NULL, serveThreadTask, tArgs)) {
        return SERVER_FAILURE;
    }
    pthread_detach(tid);
    return SERVER_SUCCESS;
}

/* Serve una richiesta in modalità multiprocesso. */
static int serveProc(int client, const logger_t* pLogger, const server_t* pServer) {
    pid_t pid = fork();
    if (pid < 0) {
        return SERVER_FAILURE;
    } else if (pid == 0) {
        gopher(client, pServer->port, pLogger);
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
        if (PLATFORM_FAILURE == closeSocket(pServer->sock)) {
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
        port = strtol(portBuff, &strtolptr, 10);
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

int runServer(server_t* pServer, logger_t* pLogger) {
    fd_set incomingConnections;
    int ready = 0;
    printHeading(pServer);
    while (true) {
        do {
            if (requestShutdown) {
                return SERVER_SUCCESS;
            } else if (SOCKET_ERROR == ready && sockErr() != EINTR) {
                return SERVER_FAILURE;
            } else if (updateConfig) {
                printf("Updating config...\n");
                int prevMultiprocess = pServer->multiProcess;
                if (readConfig(pServer, READ_PORT | READ_MULTIPROCESS) != 0) {
                    _logErr(WARN " - " _CONFIG_ERR);
                    defaultConfig(pServer, READ_PORT);
                }
                if (pServer->port != htons(pServer->sockAddr.sin_port)) {
                    if (SERVER_FAILURE == prepareSocket(pServer, SERVER_UPDATE)) {
                        return SERVER_FAILURE;
                    }
                    printf("Switched to port %i\n", pServer->port);
                }
                if (pServer->multiProcess != prevMultiprocess) {
                    printf("Switched to %s mode\n", pServer->multiProcess ? "multiprocess" : "multithreaded");
                }
                updateConfig = false;
            }
            FD_ZERO(&incomingConnections);
            FD_SET(pServer->sock, &incomingConnections);
            FD_SET(awakeSelect, &incomingConnections);
        } while ((ready = select(pServer->sock + 1, &incomingConnections, NULL, NULL, NULL)) < 0 || !FD_ISSET(pServer->sock, &incomingConnections));
        printf("Incoming connection on port %d\n", pServer->port);
        socket_t client = accept(pServer->sock, NULL, NULL);
        if (INVALID_SOCKET == client) {
            _logErr(WARN "Error serving client");
        } else if (pServer->multiProcess) {
            // TODO controllare return value
            serveProc(client, pLogger, pServer);
            closeSocket(client);
        } else {
            serveThread(client, pServer->port, pLogger);
        }
    }
    return SERVER_SUCCESS;
}