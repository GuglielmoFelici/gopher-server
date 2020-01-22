#include "../headers/gopher.h"
#include "../headers/server.h"
#include "../headers/wingetopt.h"

int main(int argc, _string* argv) {
    fd_set incomingConnections;
    int addrLen, errorCode, ready, port;
    //_socket server;
    server_t server = {.port = INVALID_PORT,  // TODO come inizializzare?
                       .multiProcess = INVALID_MULTIPROCESS};
    // TODO atexit(_shutdown); sicuro??
    if (GOPHER_SUCCESS != getCwd(server.installationDir, sizeof(server.installationDir))) {
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
                port = strtol(optarg, &strtolPtr, 10);
                if (port < 1 || port > 65535) {
                    fprintf(stderr, "Invalid port number: %s\n", optarg);
                    exit(1);
                } else {
                    server.port = port;
                }
                break;
            case 'd':
                if (GOPHER_SUCCESS != changeCwd(optarg)) {
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

    if (startup() != SERVER_SUCCESS) {
        _err(_STARTUP_ERR, true, -1);
    }

    /* Configurazione */
    installDefaultSigHandlers();
    if (startTransferLog() != GOPHER_SUCCESS) {
        printf(WARN " - Error starting logger\n");
    }
    server = prepareServer(SERVER_INIT, &options, &serverAddr);
    printHeading(&options);

    /* Main loop*/

    ready = 0;
    while (true) {
        do {
            if (requestShutdown) {
                _shutdown(server);
            } else if (ready == SOCKET_ERROR && sockErr() != EINTR) {
                _err(_SELECT_ERR, true, -1);
            } else if (updateConfig) {
                printf("Updating config...\n");
                int prevMultiprocess = options.multiProcess;
                if (readConfig(&options, READ_PORT | READ_MULTIPROCESS) != 0) {
                    _logErr(WARN " - " _CONFIG_ERR);
                    defaultConfig(&options, READ_PORT);
                }
                if (options.port != htons(serverAddr.sin_port)) {
                    server = prepareServer(server, &options, &serverAddr);
                    printf("Switched to port %i\n", options.port);
                }
                if (options.multiProcess != prevMultiprocess) {
                    printf("Switched to %s mode\n", options.multiProcess ? "multiprocess" : "multithreaded");
                }
                updateConfig = false;
            }
            FD_ZERO(&incomingConnections);
            FD_SET(server, &incomingConnections);
            FD_SET(awakeSelect, &incomingConnections);
        } while ((ready = select(server + 1, &incomingConnections, NULL, NULL, NULL)) < 0 || !FD_ISSET(server, &incomingConnections));
        printf("Incoming connection on port %d\n", htons(serverAddr.sin_port));
        _socket client = accept(server, NULL, NULL);
        if (client == INVALID_SOCKET) {
            _logErr(WARN "Error serving client");
        } else if (options.multiProcess) {
            // TODO controllare return value
            serveProc(client, options.port);
            closeSocket(client);
        } else {
            serveThread(client, options.port);
        }
    }
}
