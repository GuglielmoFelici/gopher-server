#include <stdio.h>
#include <string.h>

#include "../headers/globals.h"
#include "../headers/log.h"
#include "../headers/logger.h"
#include "../headers/platform.h"
#include "../headers/server.h"
#include "../headers/wingetopt.h"

char installDir[MAX_NAME];

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
    bool portOk = true, mpOk = true;
    if (SERVER_SUCCESS != readConfig(&server, READ_PORT)) {
        portOk = false;
        defaultConfig(&server, READ_PORT);
    }
    if (SERVER_SUCCESS != readConfig(&server, READ_MULTIPROCESS)) {
        mpOk = false;
        defaultConfig(&server, READ_MULTIPROCESS);
    }
    /* Options parsing */
    int opt, opterr = 0;
    while ((opt = getopt(argc, argv, "hmsp:d:")) != -1) {
        switch (opt) {
            case 'h':
                logMessage(MAIN_USAGE, LOG_INFO);
                goto ON_ERROR;
            case 's':
                enableLogging = false;
            case 'm':
                mpOk = true;
                server.multiProcess = true;
                break;
            case 'p':
                portOk = true;
                if (optarg[0] == '-') {
                    logMessage(MAIN_USAGE, LOG_INFO);
                    goto ON_ERROR;
                }
                int port = strtol(optarg, NULL, 10);
                if (port < 1 || port > 65535) {
                    logMessage(MAIN_PORT_ERR, LOG_WARNING);
                } else {
                    server.port = port;
                }
                break;
            case 'd':
                if (PLATFORM_SUCCESS != changeCwd(optarg)) {
                    logMessage(MAIN_CWD_ERR, LOG_WARNING);
                }
                break;
            default:
                logMessage(MAIN_USAGE, LOG_INFO);
                goto ON_ERROR;
        }
    }
    if (!portOk) {
        logMessage(MAIN_PORT_CONFIG_ERR, LOG_WARNING);
    }
    if (!mpOk) {
        logMessage(MAIN_MULTIPROCESS_CONFIG_ERR, LOG_WARNING);
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
        fprintf(stderr, "The program terminated with errors, check logs.\n");
        logMessage(TERMINATE_WITH_ERRORS, LOG_ERR);
    }
    stopTransferLog(&logger);
    return 1;
}
