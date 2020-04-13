#include <stdio.h>
#include <string.h>

#include "../headers/globals.h"
#include "../headers/log.h"
#include "../headers/logger.h"
#include "../headers/platform.h"
#include "../headers/server.h"
#include "../headers/wingetopt.h"

char installDir[MAX_NAME];
char configPath[MAX_NAME] = CONFIG_DEFAULT;

int main(int argc, string_t* argv) {
    server_t server;
    logger_t logger;
    if (SERVER_SUCCESS != initWsa()) {
        logMessage(MAIN_WSA_ERR, LOG_ERR);
        goto ON_ERROR;
    }
    if (PLATFORM_SUCCESS != getCwd(installDir, sizeof(installDir))) {
        logMessage(MAIN_CWD_ERR, LOG_ERR);
        goto ON_ERROR;
    }
    logger.pid = -1;
    defaultConfig(&server, READ_MULTIPROCESS | READ_PORT);
    /* Options parsing */
    int opt, opterr = 0;
    bool port = false, mp = false, silent = false, config = false;
    while ((opt = getopt(argc, argv, "hmsp:d:f:")) != -1) {
        switch (opt) {
            case 'h':
                fprintf(stderr, "%s\n", MAIN_USAGE);
                goto ON_ERROR;
            case 'f':
                if (strlen(optarg) >= sizeof(configPath)) {
                    fprintf(stderr, "%s\n", PATH_TOO_LONG);
                    goto ON_ERROR;
                }
                snprintf(configPath, sizeof(configPath), "%s", optarg);
                config = true;
                break;
            case 's':
                enableLogging = false;
                silent = true;
                break;
            case 'm':
                mp = true;
                server.multiProcess = true;
                break;
            case 'p':
                port = true;
                if (optarg[0] == '-') {
                    fprintf(stderr, "%s\n", MAIN_USAGE);
                    goto ON_ERROR;
                }
                int port = strtol(optarg, NULL, 10);
                server.port = port;
                break;
            case 'd':
                if (PLATFORM_SUCCESS != changeCwd(optarg)) {
                    fprintf(stderr, "%s\n", MAIN_CWD_ERR);
                }
                break;
            default:
                fprintf(stderr, "%s\n", MAIN_USAGE);
                goto ON_ERROR;
        }
    }
    if (config) {
        int request = 0 | (port ? 0 : READ_PORT) | (mp ? 0 : READ_MULTIPROCESS) | (silent ? 0 : READ_SILENT);
        if (SERVER_SUCCESS != readConfig(&server, request)) {
            fprintf(stderr, "%s\n", MAIN_CONFIG_ERR);
            defaultConfig(&server, request);
        }
    }
    if (server.port < 1 || server.port > 65535) {
        fprintf(stderr, "%s\n", MAIN_PORT_ERR);
    }
    if (enableLogging) {
        printf("Port %d\n", server.port);
        logMessage(MAIN_STARTING, LOG_INFO);
    }
    if (PLATFORM_SUCCESS != daemonize()) {
        logMessage(MAIN_DAEMON_ERR, LOG_ERR);
        goto ON_ERROR;
    }
    logger_t* pLogger = (startTransferLog(&logger) == LOGGER_SUCCESS ? &logger : NULL);
    if (!pLogger) {
        logMessage(MAIN_START_LOG_ERR, LOG_WARNING);
    }
    if (SERVER_SUCCESS != prepareSocket(&server, SERVER_INIT)) {
        logMessage(MAIN_SOCKET_ERR, LOG_ERR);
        goto ON_ERROR;
    }
    // Avvio
    if (SERVER_SUCCESS != runServer(&server, pLogger)) {
        logMessage(MAIN_LOOP_ERR, LOG_ERR);
        goto ON_ERROR;
    }
    closeSocket(server.sock);
    stopTransferLog(&logger);
    return 0;
ON_ERROR:
    if (enableLogging) {
        logMessage(TERMINATE_WITH_ERRORS, LOG_ERR);
    }
    stopTransferLog(&logger);
    return 1;
}
