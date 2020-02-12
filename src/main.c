#include <stdio.h>
#include "../headers/log.h"
#include "../headers/logger.h"
#include "../headers/platform.h"
#include "../headers/server.h"
#include "../headers/wingetopt.h"

#define CONFIG_FILE "config"

int main(int argc, string_t* argv) {
    server_t server;
    logger_t logger;
    /* Inizializzazione delle strutture di configurazione */
    if (SERVER_SUCCESS != initServer(&server)) {
        logMessage(MAIN_STARTUP_ERR, LOG_ERR);
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
            case 'p':;
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
    logger_t* pLogger = (startTransferLog(&logger) == LOGGER_SUCCESS ? &logger : NULL);
    if (!pLogger) {
        logMessage(MAIN_START_LOG_ERR, LOG_WARNING);
    }
    /* Configurazione ambiente */
    if (SERVER_SUCCESS != prepareSocket(&server, SERVER_INIT)) {
        logMessage(MAIN_SOCKET_ERR, LOG_ERR);
        goto ON_ERROR;
    }
    if (SERVER_SUCCESS != installDefaultSigHandlers()) {
        logMessage(MAIN_CTRL_ERR, LOG_ERR);
        goto ON_ERROR;
    }
    if (PLATFORM_SUCCESS != daemonize()) {  // TODO spostare?
        logMessage(MAIN_STARTUP_ERR, LOG_ERR);
        goto ON_ERROR;
    }
    /* Avvio */
    if (SERVER_SUCCESS != runServer(&server, pLogger)) {
        logMessage(MAIN_STARTUP_ERR, LOG_ERR);
        goto ON_ERROR;
    }
    destroyServer(&server);
    stopLogger(&logger);
    logMessage("Done.", LOG_INFO);
    return 0;
ON_ERROR:
    stopLogger(&logger);
    destroyServer(&server);
    stopLogger(&logger);
    return 1;
}
