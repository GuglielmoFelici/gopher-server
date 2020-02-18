#include <stdio.h>
#include <string.h>
#include "../headers/log.h"
#include "../headers/logger.h"
#include "../headers/platform.h"
#include "../headers/server.h"
#include "../headers/wingetopt.h"

int main(int argc, string_t* argv) {
    server_t server;
    logger_t logger;
    logMessage(MAIN_STARTING, LOG_INFO);
    // Inizializzazione delle strutture di configurazione
    if (SERVER_SUCCESS != initWsa()) {
        logMessage(MAIN_WSA_ERR, LOG_ERR);
        goto ON_ERROR;
    }
    if (PLATFORM_SUCCESS != getCwd(server.installationDir, sizeof(server.installationDir))) {
        logMessage(MAIN_CWD_ERR, LOG_ERR);
        goto ON_ERROR;
    }
    snprintf(server.configFile, sizeof(server.configFile), "%s/%s", server.installationDir, CONFIG_FILE);
    initLogger(&logger);
    strncpy(logger.installationDir, server.installationDir, sizeof(logger.installationDir));
    if (SERVER_SUCCESS != readConfig(&server, READ_PORT)) {
        logMessage(MAIN_PORT_CONFIG_ERR, LOG_WARNING);
        defaultConfig(&server, READ_PORT);
    }
    if (SERVER_SUCCESS != readConfig(&server, READ_MULTIPROCESS)) {
        logMessage(MAIN_MULTIPROCESS_CONFIG_ERR, LOG_WARNING);
        defaultConfig(&server, READ_MULTIPROCESS);
    }
    /* Parsing opzioni */
    int opt, opterr = 0;
    while ((opt = getopt(argc, argv, "mhp:d:")) != -1) {
        switch (opt) {
            case 'h':
                logMessage(MAIN_USAGE, LOG_INFO);
                goto ON_ERROR;
            case 'm':
                server.multiProcess = true;
                break;
            case 'p':
                if (optarg[0] == '-') {
                    logMessage(MAIN_USAGE, LOG_INFO);
                    goto ON_ERROR;
                }
                int port = strtol(optarg, NULL, 10);
                if (port < 1 || port > 65535) {
                    logMessage(MAIN_PORT_CONFIG_ERR, LOG_WARNING);
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
    // Configurazione ambiente
    if (SERVER_SUCCESS != prepareSocket(&server, SERVER_INIT)) {
        logMessage(MAIN_SOCKET_ERR, LOG_ERR);
        goto ON_ERROR;
    }
    printf("Port %d\n", server.port);
    if (PLATFORM_SUCCESS != daemonize()) {
        logMessage(MAIN_STARTUP_ERR, LOG_ERR);
        goto ON_ERROR;
    }
    logger_t* pLogger = (startTransferLog(&logger) == LOGGER_SUCCESS ? &logger : NULL);
    if (!pLogger) {
        logMessage(MAIN_START_LOG_ERR, LOG_WARNING);
    }
    if (SERVER_SUCCESS != installDefaultSigHandlers()) {
        logMessage(MAIN_CTRL_ERR, LOG_ERR);
        goto ON_ERROR;
    }
    // Avvio
    if (SERVER_SUCCESS != runServer(&server, pLogger)) {
        logMessage(MAIN_STARTUP_ERR, LOG_ERR);
        goto ON_ERROR;
    }
    closeSocket(server.sock);
    stopLogger(&logger);
    return 0;
ON_ERROR:
    fprintf(stderr, "The program terminated with errors, check logs.\n");
    logMessage(TERMINATE_WITH_ERRORS, LOG_ERR);
    stopLogger(&logger);
    return 1;
}
