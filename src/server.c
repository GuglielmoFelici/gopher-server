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

#if defined(_WIN32)

/*****************************************************************************************************************/
/*                                             WINDOWS FUNCTIONS                                                 */

/*****************************************************************************************************************/

#define SEL_TIMEOUT \
    (struct timeval) { 1, 0 }

void printHeading(const server_t* pServer) {
    printf("Listening on port %d (%s mode)\n", pServer->port, pServer->multiProcess ? "multiprocess" : "multithreaded");
}

/* Inizializza la WSA */
int initServer(server_t* pServer) {
    if (!pServer) {
        return SERVER_FAILURE;
    }
    strncpy(pServer->installationDir, "", sizeof(pServer->installationDir));
    pServer->sock = INVALID_SOCKET;
    memset(&(pServer->sockAddr), 0, sizeof(&(pServer->sockAddr)));
    WSADATA wsaData;
    WORD versionWanted = MAKEWORD(1, 1);
    return WSAStartup(versionWanted, &wsaData) == 0 ? SERVER_SUCCESS : SERVER_FAILURE;
}

int destroyServer(server_t* pServer) {
    memset(&(pServer->sockAddr), 0, sizeof(struct sockaddr_in));
    return closesocket(pServer->sock) == 0 ? SERVER_SUCCESS : SERVER_FAILURE;
}

/********************************************** SIGNALS *************************************************************/

/* Termina graziosamente il programma. */
static BOOL WINAPI ctrlC(DWORD signum) {
    return requestShutdown = (signum == CTRL_C_EVENT);
}

/* Richiede la rilettura del file di configurazione */
static BOOL WINAPI ctrlBrk(DWORD signum) {
    updateConfig = (signum == CTRL_BREAK_EVENT);
}

/* Installa i gestori di eventi */
int installDefaultSigHandlers() {
    return SetConsoleCtrlHandler(ctrlC, TRUE) && SetConsoleCtrlHandler(ctrlBrk, TRUE) ? SERVER_SUCCESS : SERVER_FAILURE;
}

/*********************************************** THREADS & PROCESSES ***************************************************************/

/* Task lanciato dal server per avviare un thread che esegue il protocollo Gopher. */
static DWORD WINAPI serveThreadTask(LPVOID args) {
    SetConsoleCtrlHandler(ctrlBrk, FALSE);
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
    if (NULL == (thread = CreateThread(NULL, 0, serveThreadTask, args, 0, NULL))) {
        free(args);
        return SERVER_FAILURE;
    }
    CloseHandle(thread);
    return SERVER_SUCCESS;
}

/* Serve una richiesta in modalità multiprocesso. */
static int serveProc(SOCKET client, const logger_t* pLogger, const server_t* pServer) {
    HANDLE logPipe = NULL;
    LPSTR cmdLine = NULL;
    if (pLogger) {
        logPipe = pLogger->logPipe;
    }
    if (!pServer) {
        goto ON_ERROR;
    }
    char exec[MAX_NAME];
    if (snprintf(exec, sizeof(exec), "%s/" HELPER_PATH, pServer->installationDir) < strlen(pServer->installationDir) + strlen(HELPER_PATH) + 1) {
        goto ON_ERROR;
    }
    STARTUPINFO startupInfo;
    PROCESS_INFORMATION processInfo;
    memset(&startupInfo, 0, sizeof(startupInfo));
    memset(&processInfo, 0, sizeof(processInfo));
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESTDHANDLES;  // TODO rimuovere
    if (
        INVALID_HANDLE_VALUE == (startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE)) ||
        INVALID_HANDLE_VALUE == (startupInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE)) ||
        INVALID_HANDLE_VALUE == (startupInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE))) {
        goto ON_ERROR;
    }
    size_t cmdLineSize;
    cmdLineSize = snprintf(NULL, 0, "%s %hu %p %p", exec, pServer->port, client, logPipe) + 1;
    if (NULL == (cmdLine = malloc(cmdLineSize))) {
        goto ON_ERROR;
    }
    if (snprintf(cmdLine, cmdLineSize, "%s %hu %p %p", exec, pServer->port, client, logPipe) < cmdLineSize - 1) {
        goto ON_ERROR;
    }
    if (!CreateProcess(exec, cmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &startupInfo, &processInfo)) {
        goto ON_ERROR;
    }
    free(cmdLine);
    return SERVER_SUCCESS;
ON_ERROR:
    if (cmdLine) {
        free(cmdLine);
    }
    return SERVER_FAILURE;
}

#else

/*****************************************************************************************************************/
/*                                             LINUX FUNCTIONS                                                    */

/*****************************************************************************************************************/

#include <sys/time.h>

#define SEL_TIMEOUT NULL

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

int destroyServer(server_t* pServer) {
    memset(&(pServer->sockAddr), 0, sizeof(struct sockaddr_in));
    return close(pServer->sock) == 0 ? SERVER_SUCCESS : SERVER_FAILURE;
}

/********************************************** SIGNALS *************************************************************/

/* Richiede la rilettura del file di configurazione */
static void hupHandler(int signum) {
    updateConfig = true;
}
/* Richiede la terminazione */
static void intHandler(int signum) {
    printf("ciao, sono %d\n", getpid());
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
    if (!pServer) {
        goto ON_ERROR;
    }
    if (flags & SERVER_UPDATE) {
        if (PLATFORM_FAILURE == closeSocket(pServer->sock)) {
            goto ON_ERROR;
        };
    }
    if (INVALID_SOCKET == (pServer->sock = socket(AF_INET, SOCK_STREAM, 0))) {
        goto ON_ERROR;
    }
    pServer->sockAddr.sin_family = AF_INET;
    pServer->sockAddr.sin_addr.s_addr = INADDR_ANY;
    pServer->sockAddr.sin_port = htons(pServer->port);
    if (bind(pServer->sock, (struct sockaddr*)&(pServer->sockAddr), sizeof(pServer->sockAddr)) < 0) {
        goto ON_ERROR;
    }
    if (listen(pServer->sock, 5) < 0) {
        goto ON_ERROR;
    }
    return SERVER_SUCCESS;
ON_ERROR:
    if (pServer) {
        closesocket(pServer->sock);
    }
    return SERVER_FAILURE;
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
    FILE* configFile = NULL;
    if (NULL == (configFile = fopen(pServer->configFile, "r"))) {
        goto ON_ERROR;
    }
    char portBuff[6], multiProcess[2];
    char c;
    do {
        c = fgetc(configFile);
    } while (c != CONFIG_DELIMITER);
    if (c == EOF) {
        goto ON_ERROR;
    }
    fgets(portBuff, sizeof(portBuff), configFile);
    do {
        c = fgetc(configFile);
    } while (c != CONFIG_DELIMITER);
    if (c == EOF) {
        goto ON_ERROR;
    }
    fgets(multiProcess, sizeof(multiProcess), configFile);
    if (fclose(configFile) != 0) {
        goto ON_ERROR;
    }
    if (which & READ_PORT) {
        int port;
        port = strtol(portBuff, NULL, 10);
        if (port < 1 || port > 65535) {
            goto ON_ERROR;
        }
        pServer->port = port;
    }
    if (which & READ_MULTIPROCESS) {
        pServer->multiProcess = strtol(multiProcess, NULL, 10);
    }
    return SERVER_SUCCESS;
ON_ERROR:
    if (configFile) {
        fclose(configFile);
    }
    return SERVER_FAILURE;
}

int runServer(server_t* pServer, logger_t* pLogger) {
    if (!pServer) {
        return SERVER_FAILURE;
    }
    fd_set incomingConnections;
    struct timeval timeOut = SEL_TIMEOUT;
    int ready = 0;
    printHeading(pServer);
    while (true) {
        do {
            if (SOCKET_ERROR == ready && EINTR != sockErr()) {
                return SERVER_FAILURE;
            } else if (requestShutdown) {
                return SERVER_SUCCESS;
            } else if (updateConfig) {
                printf("Updating config\n");
                int prevMultiprocess = pServer->multiProcess;
                if (readConfig(pServer, READ_PORT | READ_MULTIPROCESS) != 0) {
                    logErr(WARN MAIN_CONFIG_ERR);
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
        } while ((ready = select(pServer->sock + 1, &incomingConnections, NULL, NULL, &timeOut)) <= 0 || !FD_ISSET(pServer->sock, &incomingConnections));
        printf("Incoming connection on port %d\n", pServer->port);
        socket_t client = accept(pServer->sock, NULL, NULL);
        if (INVALID_SOCKET == client) {
            logErr(WARN "Error serving client");
        } else if (pServer->multiProcess) {
            // TODO controllare return value
            if (SERVER_SUCCESS != serveProc(client, pLogger, pServer)) {
                logErr("Error serving client\n");
            };
            closeSocket(client);  // TODO Questo??
        } else {
            serveThread(client, pServer->port, pLogger);
        }
    }
    return SERVER_SUCCESS;
}