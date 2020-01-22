#include "../headers/gopher.h"
#include "../headers/logger.h"
#include "../headers/platform.h"
#include "../headers/server.h"
#include "../headers/wingetopt.h"

int main(int argc, _string* argv) {
    server_t server;
    if (SERVER_SUCCESS != initServer(&server)) {
        _err(_STARTUP_ERR, true, -1);
    }
    if (PLATFORM_SUCCESS != getCwd(server.installationDir, sizeof(server.installationDir))) {
        _err("Cannot get current working directory", true, -1);
    }
    /* Parsing opzioni */
    int opt, opterr = 0;
    while ((opt = getopt(argc, argv, "mhp:d:")) != -1) {
        switch (opt) {
            case 'h':
                printf(USAGE "\n");
                exit(0);
            case 'm':
                server.multiProcess = true;
                break;
            case 'p':;
                char* strtolPtr = NULL;
                int port = strtol(optarg, &strtolPtr, 10);
                if (port < 1 || port > 65535) {
                    fprintf(stderr, "Invalid port number: %s\n", optarg);
                    exit(1);
                } else {
                    server.port = port;
                }
                break;
            case 'd':
                if (PLATFORM_SUCCESS != changeCwd(optarg)) {
                    fprintf(stderr, "Can't change directory, default one will be used\n", optarg);
                }
                break;
            case '?':
                if (optopt == 'd' || optopt == 'p')
                    fprintf(stderr, "Option -%c requires an argument (use -h for usage info).\n", optopt);
                else
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                exit(1);
            default:
                abort();
        }
    }
    if (INVALID_PORT == server.port) {
        if (SERVER_SUCCESS != readConfig(&server, READ_PORT)) {
            _logErr(WARN " - " _PORT_CONFIG_ERR);
            defaultConfig(&server, READ_PORT);
        }
    }
    if (INVALID_MULTIPROCESS == server.multiProcess) {
        if (SERVER_SUCCESS != readConfig(&server, READ_MULTIPROCESS)) {
            _logErr(WARN " - " _MULTIPROCESS_CONFIG_ERR);
            defaultConfig(&server, READ_MULTIPROCESS);
        }
    }

    // TODO daemonize ?

    /* Configurazione */
    if (SERVER_FAILURE == installDefaultSigHandlers()) {
        _err(_SYS_ERR, true, -1);
    }
    logger_t logger;
    if (LOGGER_SUCCESS != startTransferLog(&logger)) {
        printf(WARN " - Error starting logger\n");
    }
    if (SERVER_FAILURE != prepareSocket(&server, SERVER_INIT)) {
        _err(_SOCKET_ERR, true, -1);
    }
    printHeading(&server);
    if (runServer(&server) != SERVER_SUCCESS) {
        _err(_STARTUP_ERR, true, -1);
    }
}
