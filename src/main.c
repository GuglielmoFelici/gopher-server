#include <stdio.h>
#include <string.h>

#include "../headers/globals.h"
#include "../headers/log.h"
#include "../headers/logger.h"
#include "../headers/platform.h"
#include "../headers/server.h"
#include "../headers/wingetopt.h"

string_t configPath = NULL;
string_t logPath = NULL;
string_t winLoggerPath = NULL;
string_t winHelperPath = NULL;

int main(int argc, string_t* argv) {
    server_t server;
    logger_t logger;
    char newWD[MAX_NAME] = "";
    if (SERVER_SUCCESS != initWsa()) {
        fprintf(stderr, "%s\n", MAIN_WSA_ERR);
        goto ON_ERROR;
    }
    if (PLATFORM_SUCCESS != getWindowsHelpersPaths()) {
        fprintf(stderr, "%s\n", HELPER_OPEN_ERR);
        goto ON_ERROR;
    }
    logger.pid = -1;
    defaultConfig(&server, READ_MULTIPROCESS | READ_PORT);
    /* Options parsing */
    int opt, opterr = 0;
    bool port = false, mp = false, silent = false;
    while ((opt = getopt(argc, argv, "hmsp:d:f:l:")) != -1) {
        switch (opt) {
            case 'h':
                fprintf(stderr, "%s\n", MAIN_USAGE);
                goto ON_ERROR;
            case 'f':
                if (strlen(optarg) > MAX_NAME) {
                    fprintf(stderr, "%s\n", CONFIG_OPEN_ERR);
                    goto ON_ERROR;
                }
                configPath = realpath(optarg, NULL);
                if (NULL == configPath) {
                    fprintf(stderr, "%s\n", CONFIG_OPEN_ERR);
                    goto ON_ERROR;
                }
                break;
            case 's':
                enableLogging = false;
                silent = true;
                break;
            case 'l':
                if (strlen(optarg) > MAX_NAME) {
                    fprintf(stderr, "%s\n", LOGFILE_OPEN_ERR);
                    goto ON_ERROR;
                }
                logPath = realpath(optarg, NULL);
                if (NULL == logPath) {
                    if (PLATFORM_SUCCESS != createIfAbsent(optarg)) {
                        fprintf(stderr, "%s\n", LOGFILE_OPEN_ERR);
                        goto ON_ERROR;
                    }
                    if (NULL == (logPath = realpath(optarg, NULL))) {
                        fprintf(stderr, "%s\n", LOGFILE_OPEN_ERR);
                        goto ON_ERROR;
                    }
                }
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
                if (server.port < 1 || server.port > 65535) {
                    fprintf(stderr, "%s\n", MAIN_PORT_ERR);
                } else {
                    server.port = port;
                }
                break;
            case 'd':
                strncpy(newWD, optarg, sizeof(newWD));
                break;
            default:
                fprintf(stderr, "%s\n", MAIN_USAGE);
                goto ON_ERROR;
        }
    }
    if (configPath) {
        int request = 0 | (port ? 0 : READ_PORT) | (mp ? 0 : READ_MULTIPROCESS) | (silent ? 0 : READ_SILENT);
        if (SERVER_SUCCESS != readConfig(&server, request)) {
            fprintf(stderr, "%s\n", MAIN_CONFIG_ERR);
            defaultConfig(&server, request);
        }
    }
    // TODO aggiungere al config file
    if (!logPath) {
        if (PLATFORM_SUCCESS != createIfAbsent(LOG_FILE)) {
            fprintf(stderr, "%s\n", LOGFILE_OPEN_ERR);
            goto ON_ERROR;
        }
        if (NULL == (logPath = realpath(LOG_FILE, NULL))) {
            fprintf(stderr, "%s\n", LOGFILE_OPEN_ERR);
            goto ON_ERROR;
        }
    }
    if (strlen(newWD)) {
        if (PLATFORM_SUCCESS != changeCwd(newWD)) {
            fprintf(stderr, "%s\n", MAIN_CWD_ERR);
        }
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
    return 0;
ON_ERROR:
    if (enableLogging) {
        logMessage(TERMINATE_WITH_ERRORS, LOG_ERR);
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
