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
        logErr(MAIN_STARTUP_ERR);
        goto ON_ERROR;
    }
    if (PLATFORM_SUCCESS != getCwd(server.installationDir, sizeof(server.installationDir))) {
        logErr(MAIN_CWD_ERR);
        goto ON_ERROR;
    }
    snprintf(server.configFile, sizeof(server.configFile), "%s/%s", server.installationDir, CONFIG_FILE);
    initLogger(&logger);
    strncpy(logger.installationDir, server.installationDir, sizeof(logger.installationDir));
    if (SERVER_SUCCESS != readConfig(&server, READ_PORT)) {
        logErr(WARN " - " MAIN_PORT_CONFIG_ERR);
        defaultConfig(&server, READ_PORT);
    }
    if (SERVER_SUCCESS != readConfig(&server, READ_MULTIPROCESS)) {
        logErr(WARN " - " MAIN_MULTIPROCESS_CONFIG_ERR);
        defaultConfig(&server, READ_MULTIPROCESS);
    }
    /* Parsing opzioni */
    int opt, opterr = 0;
    while ((opt = getopt(argc, argv, "mhp:d:")) != -1) {
        switch (opt) {
            case 'h':
                logErr(MAIN_USAGE);
                goto ON_ERROR;
            case 'm':
                server.multiProcess = true;
                break;
            case 'p':;
                int port = strtol(optarg, NULL, 10);
                if (port < 1 || port > 65535) {
                    logErr(MAIN_PORT_CONFIG_ERR);
                } else {
                    server.port = port;
                }
                break;
            case 'd':
                if (PLATFORM_SUCCESS != changeCwd(optarg)) {
                    logErr(WARN MAIN_CWD_ERR);
                }
                break;
            default:
                logErr(MAIN_USAGE);
                goto ON_ERROR;
        }
    }
    logger_t* pLogger = (startTransferLog(&logger) == LOGGER_SUCCESS ? &logger : NULL);
    if (!pLogger) {
        logErr(WARN MAIN_START_LOG_ERR);
    }
    /* Configurazione ambiente */
    if (SERVER_SUCCESS != prepareSocket(&server, SERVER_INIT)) {
        logErr(MAIN_SOCKET_ERR);
        goto ON_ERROR;
    }
    if (SERVER_SUCCESS != installDefaultSigHandlers()) {
        logErr(MAIN_CTRL_ERR);
        goto ON_ERROR;
    }
    if (PLATFORM_SUCCESS != daemonize()) {  // TODO spostare?
        logErr(MAIN_STARTUP_ERR);
        goto ON_ERROR;
    }
    /* Avvio */
    if (SERVER_SUCCESS != runServer(&server, pLogger)) {
        logErr(MAIN_STARTUP_ERR);
        goto ON_ERROR;
    }
    destroyServer(&server);
    stopLogger(&logger);
    printf("Done.\n");
    return 0;
ON_ERROR:
    stopLogger(&logger);
    destroyServer(&server);
    stopLogger(&logger);
    return 1;
}
