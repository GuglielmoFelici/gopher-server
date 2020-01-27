#include <stdio.h>
#include "../headers/log.h"
#include "../headers/logger.h"
#include "../headers/platform.h"
#include "../headers/server.h"
#include "../headers/wingetopt.h"

#define MAX_ERR 100
#define CONFIG_FILE "config"

int main(int argc, string_t* argv) {
    server_t server;
    logger_t logger;
    char errorMsg[MAX_ERR] = "";
    if (SERVER_SUCCESS != initServer(&server)) {
        strncpy(errorMsg, _STARTUP_ERR, sizeof(errorMsg));
        goto ON_ERROR;
    }
    initLogger(&logger);
    if (PLATFORM_SUCCESS != getCwd(server.installationDir, sizeof(server.installationDir))) {
        strncpy(errorMsg, "Cannot get current working directory", sizeof(errorMsg));
        goto ON_ERROR;
    }
    strncpy(logger.installationDir, server.installationDir, sizeof(logger.installationDir));
    snprintf(server.configFile, sizeof(server.configFile), "%s/%s", server.installationDir, CONFIG_FILE);
    if (SERVER_SUCCESS != readConfig(&server, READ_PORT)) {
        logErr(WARN " - " _PORT_CONFIG_ERR);
        defaultConfig(&server, READ_PORT);
    }
    if (SERVER_SUCCESS != readConfig(&server, READ_MULTIPROCESS)) {
        logErr(WARN " - " _MULTIPROCESS_CONFIG_ERR);
        defaultConfig(&server, READ_MULTIPROCESS);
    }
    /* Parsing opzioni */
    int opt, opterr = 0;
    while ((opt = getopt(argc, argv, "mhp:d:")) != -1) {
        switch (opt) {
            case 'h':
                strncpy(errorMsg, USAGE, sizeof(errorMsg));
                goto ON_ERROR;
            case 'm':
                server.multiProcess = true;
                break;
            case 'p':;
                int port = strtol(optarg, NULL, 10);
                if (port < 1 || port > 65535) {
                    strncpy(errorMsg, "Invalid port number", sizeof(errorMsg));
                    goto ON_ERROR;
                } else {
                    server.port = port;
                }
                break;
            case 'd':
                if (PLATFORM_SUCCESS != changeCwd(optarg)) {
                    logErr(WARN " - Can't change directory, default one will be used");
                }
                break;
            case '?':
                strncpy(errorMsg, USAGE, sizeof(errorMsg));
                goto ON_ERROR;
            default:
                strncpy(errorMsg, USAGE, sizeof(errorMsg));
                goto ON_ERROR;
        }
    }
    /* Configurazione */
    if (SERVER_SUCCESS != installDefaultSigHandlers()) {
        strncpy(errorMsg, _SYS_ERR, sizeof(errorMsg));
        goto ON_ERROR;
    }
    if (SERVER_SUCCESS != prepareSocket(&server, SERVER_INIT)) {
        strncpy(errorMsg, _SOCKET_ERR, sizeof(errorMsg));
        goto ON_ERROR;
    }
    logger_t* pLogger = (startTransferLog(&logger) == LOGGER_SUCCESS ? &logger : NULL);
    if (!pLogger) {
        logErr(WARN " - Error starting logger\n");
    }
    if (PLATFORM_SUCCESS != daemonize()) {
        strncpy(errorMsg, _STARTUP_ERR, sizeof(errorMsg));
        goto ON_ERROR;
    }
    if (SERVER_SUCCESS != runServer(&server, pLogger)) {
        strncpy(errorMsg, _STARTUP_ERR, sizeof(errorMsg));
        goto ON_ERROR;
    }
    destroyServer(&server);
    if (LOGGER_SUCCESS != stopLogger(&logger)) {
        logErr(WARN " - Error closing logger\n");
    }
    printf("Done.\n");
    return 0;
ON_ERROR:
    logErr(errorMsg);
    stopLogger(&logger);
    destroyServer(&server);
    stopLogger(&logger);
    return 1;
}
