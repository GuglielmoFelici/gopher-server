#include "../headers/server.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../headers/datatypes.h"
#include "../headers/globals.h"
#include "../headers/log.h"
#include "../headers/logger.h"
#include "../headers/platform.h"
#include "../headers/protocol.h"

#define CHECK_CONFIG 0x0001
#define CHECK_SHUTDOWN 0x0002

static sig_atomic volatile updateConfig = false;
static sig_atomic volatile requestShutdown = false;
static sig_atomic volatile isLoggerActive = true;

/** A struct defining the arguments required by the children threads to serve the client*/
typedef struct {
    /** The socket of the client */
    socket_t sock;
    /** The port of the server where the request is being served */
    int port;
    /** A pointer to a logger_t representing a logger process */
    const logger_t* pLogger;
} server_thread_args_t;

/** Atomically checks if a config update or a shutdown has been requested.
 * @param which Can be CHECK_CONFIG or CHECK_SHUTDOWN, depending on the check to perform.
 * @return true if the specified request has been issued 
 */
static bool checkSignal(int which);

/** Serves a request with a new thread.
 * @param sock The socket of the client.
 * @param port The port where the server has been contacted.
 * @param pLogger A pointer to the logger_t struct representing the logging process (can be NULL).
 */
static int serveThread(socket_t sock, int port, logger_t* pLogger);

/** Servest a request with a new process. 
 * @see serveThread()
*/
static int serveProc(socket_t client, const logger_t* pLogger, const server_t* pServer);

/** Installs the default signal handlers for the server process.
 * On Windows, CTRL_C interrupts the server and CTRL_BREAK requests a configuration update.
 * On Linux, SIGINT interrupts the server and SIGHUP requests a configuration update.
 * @return SERVER_SUCCESS or SERVER_FAILURE. 
*/
int installDefaultSigHandlers();

int prepareSocket(server_t* pServer, int flags) {
    if (!pServer) {
        goto ON_ERROR;
    }
    if (flags & SERVER_UPDATE) {
        if (PLATFORM_FAILURE == closeSocket(pServer->sock)) {
            goto ON_ERROR;
        }
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
    if (pServer && pServer->sock != INVALID_SOCKET) {
        closeSocket(pServer->sock);
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
    char line[MAX_NAME];
    char* newCwd = NULL;
    int ret = SERVER_SUCCESS;
    if (NULL == (configFile = fopen(configPath, "r"))) {
        return SERVER_FAILURE;
    }
    char key[32], value[MAX_NAME];
    while (NULL != fgets(line, sizeof(line), configFile)) {
        if (sscanf(line, "%s = %s", key, value) == 2) {
            if (strcmp(key, CONFIG_PORT_KEY) == 0 && (which & READ_PORT)) {
                int port = strtol(value, NULL, 10);
                if (port < 1 || port > PORT_MAX) {
                    debugMessage(MAIN_PORT_ERR, DBG_WARN);
                    ret = SERVER_FAILURE;
                }
                pServer->port = port;
            } else if (strcmp(key, CONFIG_LOG_KEY) == 0 && (which & READ_LOG)) {
                string_t newLogPath = getRealPath(value, NULL, true);
                if (NULL == newLogPath) {
                    debugMessage(LOGFILE_OPEN_ERR, DBG_WARN);
                    ret = SERVER_FAILURE;
                } else {
                    if (logPath) free(logPath);
                    logPath = newLogPath;
                }
            } else if (strcmp(key, CONFIG_MP_KEY) == 0 && (which & READ_MULTIPROCESS)) {
                pServer->multiProcess = strcmp(value, CONFIG_YES) == 0;
            } else if (strcmp(key, CONFIG_VERB_KEY) == 0 && (which & READ_VERB)) {
                debugLevel = strtol(value, NULL, 10);
                if (debugLevel < DBG_NO || debugLevel > DBG_DEBUG) {
                    debugMessage(INVALID_DBG_LVL_ERR, DBG_ERR);
                    ret = SERVER_FAILURE;
                }
            } else if (strcmp(key, CONFIG_DIR_KEY) == 0 && (which & READ_DIR)) {
                if (NULL == (newCwd = malloc(strlen(value) + 1))) {
                    ret = SERVER_FAILURE;
                } else {
                    strncpy(newCwd, value, strlen(value) + 1);
                }
            }
        }
    }
    // Changing cwd must be last operation
    if (newCwd && PLATFORM_SUCCESS != changeCwd(newCwd)) {
        debugMessage(MAIN_CWD_ERR, DBG_ERR);
        ret = SERVER_FAILURE;
    }
    if (newCwd) free(newCwd);
    if (configFile) fclose(configFile);
    return ret;
}

int runServer(server_t* pServer, logger_t* pLogger) {
    if (!pServer) {
        return SERVER_FAILURE;
    }
    if (SERVER_SUCCESS != installDefaultSigHandlers()) {
        debugMessage(HANDLERS_ERR, DBG_ERR);
        return SERVER_FAILURE;
    }
    struct timeval dfltTimeval = {1, 0};
    struct timeval timeOut;
    fd_set incomingConnections;
    int ready = 0;
    while (true) {
        do {
            waitChildren();
            if (SOCKET_ERROR == ready && EINTR != sockErr()) {
                return SERVER_FAILURE;
            } else if (checkSignal(CHECK_SHUTDOWN)) {
                debugMessage(SHUTDOWN_REQUESTED, DBG_INFO);
                return SERVER_SUCCESS;
            } else if (checkSignal(CHECK_CONFIG) && configPath) {
                debugMessage(UPDATE_REQUESTED, DBG_INFO);
                if (SERVER_SUCCESS != readConfig(pServer, READ_PORT | READ_MULTIPROCESS | READ_VERB)) {
                    debugMessage(MAIN_CONFIG_ERR, DBG_WARN);
                }
                if (pServer->port != htons(pServer->sockAddr.sin_port)) {
                    if (SERVER_FAILURE == prepareSocket(pServer, SERVER_UPDATE)) {
                        return SERVER_FAILURE;
                    }
                }
            }
            memcpy(&timeOut, &dfltTimeval, sizeof(struct timeval));
            FD_ZERO(&incomingConnections);
            FD_SET(pServer->sock, &incomingConnections);
        } while ((ready = select(pServer->sock + 1, &incomingConnections, NULL, NULL, &timeOut)) <= 0);
        debugMessage(INCOMING_CONNECTION, DBG_DEBUG);
        if (!isLoggerActive) {
            pLogger = NULL;
        }
        socket_t client = accept(pServer->sock, NULL, NULL);
        if (INVALID_SOCKET == client) {
            debugMessage(SERVE_CLIENT_ERR, DBG_WARN);
        } else if (pServer->multiProcess) {
            if (SERVER_SUCCESS != serveProc(client, pLogger, pServer)) {
                debugMessage(SERVE_CLIENT_ERR, DBG_ERR);
            }
            closeSocket(client);
        } else {
            if (SERVER_SUCCESS != serveThread(client, pServer->port, pLogger)) {
                debugMessage(SERVE_CLIENT_ERR, DBG_ERR);
                closeSocket(client);
            }
        }
    }
    return SERVER_SUCCESS;
}

/*****************************************************************************************************************/
/*                                             WINDOWS FUNCTIONS                                                 */

/*****************************************************************************************************************/

#if defined(_WIN32)

static CRITICAL_SECTION criticalSection;

int initWsa() {
    WSADATA wsaData;
    WORD versionWanted = MAKEWORD(1, 1);
    return WSAStartup(versionWanted, &wsaData) == 0 ? SERVER_SUCCESS : SERVER_FAILURE;
}

/********************************************** SIGNALS *************************************************************/

static BOOL WINAPI ctrlHandler(DWORD signum) {
    if (signum == CTRL_C_EVENT || signum == CTRL_BREAK_EVENT) {
        EnterCriticalSection(&criticalSection);
        if (signum == CTRL_C_EVENT) {
            requestShutdown = true;
        } else {
            updateConfig = true;
        }
        LeaveCriticalSection(&criticalSection);
        return TRUE;
    } else {
        return FALSE;
    }
}

int installDefaultSigHandlers() {
    InitializeCriticalSection(&criticalSection);
    return SetConsoleCtrlHandler(ctrlHandler, TRUE) ? SERVER_SUCCESS : SERVER_FAILURE;
}

static bool checkSignal(int which) {
    bool ret = false;
    EnterCriticalSection(&criticalSection);
    if (which & CHECK_SHUTDOWN) {
        ret = requestShutdown;
    } else if (which & CHECK_CONFIG) {
        ret = updateConfig;
        updateConfig = false;
    }
    LeaveCriticalSection(&criticalSection);
    return ret;
}

/*********************************************** THREADS & PROCESSES ***************************************************************/

static DWORD WINAPI serveThreadTask(LPVOID args) {
    server_thread_args_t gopherArgs = *(server_thread_args_t*)args;
    free(args);
    gopher(gopherArgs.sock, gopherArgs.port, gopherArgs.pLogger);
}

static int serveThread(SOCKET sock, int port, logger_t* pLogger) {
    HANDLE thread;
    server_thread_args_t* args;
    if (NULL == (args = malloc(sizeof(server_thread_args_t)))) {
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

static int serveProc(SOCKET client, const logger_t* pLogger, const server_t* pServer) {
    HANDLE logPipe = NULL;
    LPSTR cmdLine = NULL;
    if (pLogger) {
        logPipe = pLogger->logPipe;
    }
    if (!pServer) {
        goto ON_ERROR;
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
        goto ON_ERROR;
    }
    size_t cmdLineSize;
    cmdLineSize = snprintf(NULL, 0, "%s %hu %p %d %p", winHelperPath, pServer->port, client, debugLevel, logPipe) + 1;
    if (NULL == (cmdLine = malloc(cmdLineSize))) {
        goto ON_ERROR;
    }
    if (snprintf(cmdLine, cmdLineSize, "%s %hu %p %d %p", winHelperPath, pServer->port, client, debugLevel, logPipe) < cmdLineSize - 1) {
        goto ON_ERROR;
    }
    if (!SetHandleInformation((HANDLE)pServer->sock, HANDLE_FLAG_INHERIT, 0)) {
        goto ON_ERROR;
    }
    if (!CreateProcess(winHelperPath, cmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &startupInfo, &processInfo)) {
        goto ON_ERROR;
    }
    free(cmdLine);
    return SERVER_SUCCESS;
ON_ERROR:
    if (cmdLine) free(cmdLine);
    return SERVER_FAILURE;
}

#else

/*****************************************************************************************************************/
/*                                             LINUX FUNCTIONS                                                    */

/*****************************************************************************************************************/

#include <pthread.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>

int initWsa() {
    return SERVER_SUCCESS;
}

/********************************************** SIGNALS *************************************************************/

static void hupHandler(int signum) {
    updateConfig = true;
}

static void intHandler(int signum) {
    requestShutdown = true;
}

static void pipeHandler(int signum) {
    isLoggerActive = false;
}

static int installSigHandler(int sig, void (*func)(int), int flags) {
    struct sigaction sigact;
    memset(&sigact, 0, sizeof(struct sigaction));
    sigact.sa_handler = func;
    sigact.sa_flags = flags;
    return sigaction(sig, &sigact, NULL) == 0 ? SERVER_SUCCESS : SERVER_FAILURE;
}

int installDefaultSigHandlers() {
    if (SERVER_SUCCESS != installSigHandler(SIGINT, &intHandler, 0)) {
        return SERVER_FAILURE;
    }
    if (SERVER_SUCCESS != installSigHandler(SIGPIPE, &pipeHandler, 0)) {
        return SERVER_FAILURE;
    }
    return installSigHandler(SIGHUP, &hupHandler, SA_NODEFER);
}

static bool checkSignal(int which) {
    if (which & CHECK_SHUTDOWN) {
        return requestShutdown;
    } else if (which & CHECK_CONFIG) {
        if (updateConfig) {
            updateConfig = false;
            return true;
        }
        return false;
    }
}

/*********************************************** THREADS & PROCESSES ***************************************************************/

static void* serveThreadTask(void* args) {
    pthread_detach(pthread_self());
    int sock;
    server_thread_args_t tArgs = *(server_thread_args_t*)args;
    free(args);
    gopher(tArgs.sock, tArgs.port, tArgs.pLogger);
}

static int serveThread(int sock, int port, logger_t* pLogger) {
    server_thread_args_t* tArgs = malloc(sizeof(server_thread_args_t));
    if (NULL == tArgs) {
        debugMessage(ARGS_ERR, DBG_ERR);
        return SERVER_FAILURE;
    }
    tArgs->sock = sock;
    tArgs->port = port;
    tArgs->pLogger = pLogger;
    pthread_t tid;
    if (pthread_create(&tid, NULL, serveThreadTask, tArgs) != 0) {
        free(tArgs);
        return SERVER_FAILURE;
    }
    return SERVER_SUCCESS;
}

static int serveProc(int client, const logger_t* pLogger, const server_t* pServer) {
    pid_t pid = fork();
    if (pid < 0) {
        return SERVER_FAILURE;
    } else if (pid == 0) {
        sigset_t set;
        if (
            sigemptyset(&set) != 0 ||
            sigaddset(&set, SIGHUP) != 0 ||
            sigaddset(&set, SIGINT) != 0 ||
            sigprocmask(SIG_BLOCK, &set, NULL) != 0) {
            exit(1);
        }
        if (close(pServer->sock) < 0) {
            exit(1);
        }
        gopher(client, pServer->port, pLogger);
        threadExit();
    } else {
        return SERVER_SUCCESS;
    }
}

#endif