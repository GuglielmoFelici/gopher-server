#include "headers/datatypes.h"
#include "headers/gopher.h"
#include "headers/log.h"
#include "headers/wingetopt.h"

// Pipe per il log
pipe logPipe;
// Socket per interrompere la select su windows
_socket awakeSelect;
struct sockaddr_in awakeAddr;
// Evento per lo shutdown del processo di log di windows
_event logEvent;
// Pid del processo di log
_procId loggerPid;
// Pid del server
_procId serverPid;
// Controllo della modifica del file di configurazione
bool updateConfig = false;
// Chiusura dell'applicazione
bool requestShutdown = false;
// Socket del server
_socket server;
// Installation directory
char installationDir[MAX_NAME] = "";

_socket prepareServer(_socket server, const struct config options, struct sockaddr_in* address) {
    if (server != -1) {
        if (closeSocket(server) < 0) {
            _err("prepareServer() - " _CLOSE_SOCKET_ERR, ERR, true, -1);
        };
    }
    if ((server = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        _err("prepareServer() - "_SOCKET_ERR, ERR, true, server);
    }
    char enable = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    address->sin_family = AF_INET;
    address->sin_addr.s_addr = INADDR_ANY;
    address->sin_port = htons(options.port);
    if (bind(server, (struct sockaddr*)address, sizeof(*address)) < 0) {
        _err("prepareServer() - "_BIND_ERR, ERR, true, -1);
    }
    if (listen(server, 5) < 0) {
        _err("prepareServer() - "_LISTEN_ERR, ERR, true, -1);
    }
    return server;
}

int main(int argc, _string* argv) {
    struct config options = {.port = 0, .multiProcess = -1};
    struct sockaddr_in serverAddr, clientAddr;
    fd_set incomingConnections;
    int addrLen, _errno, ready, port;
    char* endptr;
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    if ((_errno = startup()) != 0) {
        _err(_STARTUP_ERR, ERR, false, _errno);
    }
    atexit(_shutdown);
    // TODO controllare che la daemonizzazione non interferisca con getopt
    /* Parse options */
    int opt, opterr = 0;
    while ((opt = getopt(argc, argv, "mhp:d:")) != -1) {
        switch (opt) {
            case 'h':
                printf(USAGE "\n");
                exit(0);
            case 'm':
                options.multiProcess = 1;
                break;
            case 'p':
                port = atoi(optarg);
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
    if (options.port == 0) {
        if (readConfig(&options, READ_PORT) != 0) {
            _logErr(WARN " - " _PORT_CONFIG_ERR);
            defaultConfig(&options, READ_PORT);
        }
    }
    if (options.multiProcess == -1) {
        if (readConfig(&options, READ_MULTIPROCESS) != 0) {
            defaultConfig(&options, READ_MULTIPROCESS);
            _logErr(WARN " - " _MULTIPROCESS_CONFIG_ERR);
        }
    }

    /* Configuration */
    installSigHandler();
    startTransferLog();
    server = prepareServer(-1, options, &serverAddr);
    printf("\n*** GOPHER SERVER ***\n\n");
    printf("Listening on port %i (%s)\n", options.port, options.multiProcess ? "multiprocess mode" : "multithreaded mode");
    printf("Use CTRL-C to quit, CTRL-BREAK to refresh config file.\n");

    /* Main loop*/

    ready = 0;
    while (true) {
        do {
            if (requestShutdown) {
                exit(0);
            } else if (ready < 0 && sockErr() != EINTR) {
                _err("Server - "_SELECT_ERR, ERR, true, -1);
            } else if (updateConfig) {
                printf("Updating config...\n");
                if (readConfig(&options, READ_BOTH) != 0) {
                    _logErr(WARN " - " _CONFIG_ERR);
                    defaultConfig(&options, READ_PORT);
                } else if (options.port != htons(serverAddr.sin_port)) {
                    server = prepareServer(server, options, &serverAddr);
                }
                updateConfig = false;
            }
            FD_ZERO(&incomingConnections);
            FD_SET(server, &incomingConnections);
            FD_SET(awakeSelect, &incomingConnections);
        } while ((ready = select(server + 1, &incomingConnections, NULL, NULL, NULL)) < 0 || !FD_ISSET(server, &incomingConnections));
        printf("Incoming connection on port %d\n", htons(serverAddr.sin_port));
        addrLen = sizeof(clientAddr);
        _socket client = accept(server, (struct sockaddr*)&clientAddr, &addrLen);
        if (client == INVALID_SOCKET) {
            _logErr(WARN "Error serving client");
            continue;
        }
        if (options.multiProcess) {
            serveProc(client);
            closeSocket(client);
        } else {
            _socket* sock;
            if ((sock = malloc(sizeof(_socket))) == NULL) {
                _logErr("Error serving client: " _ALLOC_ERR);
                continue;
            }
            *sock = client;
            serveThread(sock);
        }
    }
}