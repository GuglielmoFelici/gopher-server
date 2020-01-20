#include "headers/datatypes.h"
#include "headers/gopher.h"
#include "headers/log.h"
#include "headers/wingetopt.h"

// Pipe per il log
myPipe logPipe;
// Socket per interrompere la select su windows
_socket awakeSelect;
struct sockaddr_in awakeAddr;
// Evento per lo shutdown del processo di log di windows
_event logEvent;
// Pid del processo di log
_procId loggerPid;
// Controllo della modifica del file di configurazione
_sig_atomic updateConfig = false;
// Chiusura dell'applicazione
_sig_atomic requestShutdown = false;
// Socket del server
_socket server;
// Installation directory
char installationDir[MAX_NAME] = "";

/* Inizializza il socket del server e lo mette in ascolto */
_socket prepareServer(_socket server, struct config* options, struct sockaddr_in* address) {
    ;
    if (server != SERVER_INIT) {
        if (closeSocket(server) < 0) {
            _err("prepareServer() - " _CLOSE_SOCKET_ERR, true, -1);
        };
    }
    if ((server = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        _err("prepareServer() - "_SOCKET_ERR, true, server);
    }
    address->sin_family = AF_INET;
    address->sin_addr.s_addr = INADDR_ANY;
    address->sin_port = htons(options->port);
    if (bind(server, (struct sockaddr*)address, sizeof(*address)) < 0) {
        _err("prepareServer() - "_BIND_ERR, true, -1);
    }
    if (listen(server, 5) < 0) {
        _err("prepareServer() - "_LISTEN_ERR, true, -1);
    }
    return server;
}

int main(int argc, _string* argv) {
    struct config options = {.port = INVALID_PORT, .multiProcess = INVALID_MULTIPROCESS};
    struct sockaddr_in serverAddr;
    fd_set incomingConnections;
    int addrLen, errorCode, ready, port;
    atexit(_shutdown);  // TODO sicuro??
    if (getCwd(installationDir, sizeof(installationDir)) != GOPHER_SUCCESS) {
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
                options.multiProcess = 1;
                break;
            case 'p':;
                char* strtolPtr = NULL;
                port = strtol(optarg, &strtolPtr, 10);
                if (port < 1 || port > 65535) {
                    fprintf(stderr, "Invalid port number: %s\n", optarg);
                    exit(1);
                } else {
                    options.port = port;
                }
                break;
            case 'd':
                changeCwd(optarg);
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
    if (options.port == INVALID_PORT) {
        if (readConfig(&options, READ_PORT) != 0) {
            _logErr(WARN " - " _PORT_CONFIG_ERR);
            defaultConfig(&options, READ_PORT);
        }
    }
    if (options.multiProcess == INVALID_MULTIPROCESS) {
        if (readConfig(&options, READ_MULTIPROCESS) != 0) {
            _logErr(WARN " - " _MULTIPROCESS_CONFIG_ERR);
            defaultConfig(&options, READ_MULTIPROCESS);
        }
    }

    if ((errorCode = startup()) != 0) {
        _err(_STARTUP_ERR, true, errorCode);
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
                exit(0);  // TODO
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
