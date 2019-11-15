#include "includes/datatypes.h"
#include "includes/gopher.h"
#include "includes/log.h"

// Pipe per il log
_pipe logPipe;
// Socket per interrompere la select su windows
struct sockaddr_in awakeAddr;
_socket awakeSelect;
// Evento per lo shutdown del processo di log
_event logEvent;
// Id del processo di log
_procId logger;
// Controllo della modifica del file di configurazione
bool updateConfig = false;
// Chiusura dell'applicazione
bool requestShutdown = false;
// Socket del server
_socket server;

_socket prepareServer(_socket server, const struct config options, struct sockaddr_in* address) {
    if (server != -1) {
        if (closeSocket(server) < 0) {
            _err(_CLOSE_SOCKET_ERR, ERR, true, -1);
        };
    }
    if ((server = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        _err(_SOCKET_ERR, ERR, true, server);
    }
    char enable = 1;
    if (setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        _log(_REUSE_ERR, WARN, true);
    }
    address->sin_family = AF_INET;
    address->sin_addr.s_addr = INADDR_ANY;
    address->sin_port = htons(options.port);
    if (bind(server, (struct sockaddr*)address, sizeof(*address)) < 0) {
        _err(_BIND_ERR, ERR, true, -1);
    }
    if (listen(server, 5) < 0) {
        _err(_LISTEN_ERR, ERR, true, -1);
    }
    return server;
}

int main(int argc, _string* argv) {
    struct config options;
    struct sockaddr_in serverAddr;
    struct sockaddr_in clientAddr;
    fd_set incomingConnections;
    int addrLen;
    int _errno;
    int ready;

    /* Configuration */

    if ((_errno = startup()) != 0) {
        _err(_STARTUP_ERR, ERR, false, _errno);
    }
    installSigHandler();
    // Disabilita I/O buffering
    setvbuf(stdout, NULL, _IONBF, 0);
    if ((_errno = initLog()) != 0) {
        _err(_LOG_ERR, ERR, true, _errno);
    }
    initConfig(&options);
    if (readConfig(CONFIG_FILE, &options) != 0) {
        _log(_CONFIG_ERR, WARN, true);
    }
    server = prepareServer(-1, options, &serverAddr);
    startTransferLog();
    printf("Listening on port %i\n", options.port);

    /* Main loop*/

    ready = 0;
    while (true) {
        do {
            if (requestShutdown) {
                _shutdown();
            } else if (ready < 0 && sockErr() != EINTR) {
                _err(_SELECT_ERR, ERR, true, -1);
            } else if (updateConfig) {
                printf("Reading config...\n");
                if (readConfig(CONFIG_FILE, &options) != 0) {
                    printf(_CONFIG_ERR);
                    _log(_CONFIG_ERR, WARN, true);
                } else if (options.port != htons(serverAddr.sin_port)) {
                    server = prepareServer(server, options, &serverAddr);
                }
                updateConfig = false;
            }
            FD_ZERO(&incomingConnections);
            FD_SET(server, &incomingConnections);
            FD_SET(awakeSelect, &incomingConnections);
        } while ((ready = select(server + 1, &incomingConnections, NULL, NULL, NULL)) < 0 || !FD_ISSET(server, &incomingConnections));
        printf("Incoming connection, port %d\n", htons(serverAddr.sin_port));
        addrLen = sizeof(clientAddr);
        _socket client = accept(server, (struct sockaddr*)&clientAddr, &addrLen);
        if (options.multiProcess) {
            serveProc(client);
            closeSocket(client);
        } else {
            _socket* sock;
            if ((sock = malloc(sizeof(_socket))) == NULL) {
                printf("Impossibile servire il client\n.");
                _log(_ALLOC_ERR, WARN, true);
            } else {
                *sock = client;
                serveThread(sock);
            }
        }
    }
}
