#include <stdio.h>
#include <string.h>

#include "../headers/globals.h"
#include "../headers/log.h"
#include "../headers/logger.h"
#include "../headers/platform.h"
#include "../headers/server.h"
#include "../headers/wingetopt.h"

string_t configPath = NULL;
char cwd[MAX_NAME];

struct switches {
    /* Directory, file, help, logfile, multiprocess, port, silent */
    bool d, f, h, l, m, p, s;
};

int parseOptions(int argc, string_t* argv, server_t* pServer, struct switches* pSwitches) {
    int opt, opterr = 0;
    while ((opt = getopt(argc, argv, "hmsp:d:f:l:")) != -1) {
        switch (opt) {
            case 'd':
                pSwitches->d = true;
                if (optarg[0] == '-') {
                    fprintf(stderr, "%s\n", MAIN_USAGE);
                    return -1;
                }
                strncpy(cwd, optarg, sizeof(cwd));
                break;
            case 'f':
                pSwitches->f = true;
                if (optarg[0] == '-') {
                    fprintf(stderr, "%s\n", MAIN_USAGE);
                    return -1;
                }
                configPath = getRealPath(optarg, NULL, false);
                if (NULL == configPath) {
                    fprintf(stderr, "%s\n", CONFIG_OPEN_ERR);
                    return -1;
                }
                break;
            case 'h':
                printf("%s\n", MAIN_USAGE);
                exit(0);
            case 'l':
                pSwitches->l = true;
                if (optarg[0] == '-') {
                    fprintf(stderr, "%s\n", MAIN_USAGE);
                    return -1;
                }
                logPath = getRealPath(optarg, NULL, true);
                if (NULL == logPath) {
                    fprintf(stderr, "%s\n", LOGFILE_OPEN_ERR);
                    return -1;
                }
                break;
            case 'm':
                pSwitches->m = true;
                pServer->multiProcess = true;
                break;
            case 'p':
                pSwitches->p = true;
                if (optarg[0] == '-') {
                    fprintf(stderr, "%s\n", MAIN_USAGE);
                    return -1;
                }
                int port = strtol(optarg, NULL, 10);
                if (port < 1 || port > 65535) {
                    fprintf(stderr, "%s\n", MAIN_PORT_ERR);
                } else {
                    pServer->port = port;
                }
                break;
            case 's':
                pSwitches->s = true;
                enableDebug = false;
                break;
            default:
                fprintf(stderr, "%s\n", MAIN_USAGE);
                return -1;
        }
    }
    return 0;
}

int main(int argc, string_t* argv) {
    server_t server;
    logger_t logger;
    struct switches switches;
    switches.d = switches.f = switches.l = switches.m = switches.p = switches.s = false;
    if (SERVER_SUCCESS != initWsa()) {
        fprintf(stderr, "%s\n", MAIN_WSA_ERR);
        goto ON_ERROR;
    }
    server.port = -1;
    logger.pid = -1;
    defaultConfig(&server, READ_MULTIPROCESS | READ_PORT);
    if (parseOptions(argc, argv, &server, &switches) < 0) {
        goto ON_ERROR;
    }
    // Get paths before cwd is changed
    if (PLATFORM_SUCCESS != getWindowsHelpersPaths()) {
        fprintf(stderr, "%s\n", HELPER_OPEN_ERR);
        goto ON_ERROR;
    }
    if (!switches.l) {
        if (NULL == (logPath = getRealPath(LOG_FILE, NULL, true))) {
            fprintf(stderr, "%s\n", LOGFILE_OPEN_ERR);
            goto ON_ERROR;
        }
    }
    if (switches.f) {
        int request = 0 | (switches.p ? 0 : READ_PORT) | (switches.m ? 0 : READ_MULTIPROCESS) | (switches.s ? 0 : READ_SILENT) | (switches.l ? 0 : READ_LOG) | (switches.d ? 0 : READ_DIR);
        if (SERVER_SUCCESS != readConfig(&server, request)) {
            fprintf(stderr, "%s\n", MAIN_CONFIG_ERR);
            defaultConfig(&server, request);
        }
    }
    if (switches.d) {
        if (PLATFORM_SUCCESS != changeCwd(cwd)) {
            fprintf(stderr, "%s\n", MAIN_CWD_ERR);
        }
    }
    if (enableDebug) {
        printf("Port %d\n", server.port);
        debugMessage(MAIN_STARTING, LOG_INFO);
    }
    if (PLATFORM_SUCCESS != daemonize()) {
        debugMessage(MAIN_DAEMON_ERR, LOG_ERR);
        goto ON_ERROR;
    }
    logger_t* pLogger = (startTransferLog(&logger) == LOGGER_SUCCESS ? &logger : NULL);
    if (!pLogger) {
        debugMessage(MAIN_START_LOG_ERR, LOG_WARNING);
    }
    if (SERVER_SUCCESS != prepareSocket(&server, SERVER_INIT)) {
        debugMessage(MAIN_SOCKET_ERR, LOG_ERR);
        goto ON_ERROR;
    }
    // Avvio
    if (SERVER_SUCCESS != runServer(&server, pLogger)) {
        debugMessage(MAIN_LOOP_ERR, LOG_ERR);
        goto ON_ERROR;
    }
    closeSocket(server.sock);
    stopTransferLog(&logger);
    if (configPath) {
        free(configPath);
    }
    if (logPath) {
        free(logPath);
    }
    if (winHelperPath) {
        free(winHelperPath);
    }
    return 0;
ON_ERROR:
    if (enableDebug) {
        debugMessage(TERMINATE_WITH_ERRORS, LOG_ERR);
    }
    if (configPath) {
        free(configPath);
    }
    if (logPath) {
        free(logPath);
    }
    if (winHelperPath) {
        free(logPath);
    }
    if (winLoggerPath) {
        free(logPath);
    }
    stopTransferLog(&logger);
    return 1;
}
