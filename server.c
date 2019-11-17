#include "headers/datatypes.h"
#include "headers/gopher.h"
#include "headers/log.h"

// Pipe per il log
_pipe logPipe;
// Socket per interrompere la select su windows
struct sockaddr_in awakeAddr;
_socket awakeSelect;
// Evento per lo shutdown del processo di log
_event logEvent;
// Id del processo di log
_procId loggerId;
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
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
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

    // if ((_errno = startup()) != 0) {
    //     _err(_STARTUP_ERR, ERR, false, _errno);
    // }
    installSigHandler();
    // Disabilita I/O buffering
    setvbuf(stdout, NULL, _IONBF, 0);
    initConfig(&options);
    if (readConfig(CONFIG_FILE, &options) != 0) {
        printf(_CONFIG_ERR);
    }
    startTransferLog();
    server = prepareServer(-1, options, &serverAddr);

    /* Main loop*/

    ready = 0;
    while (true) {
        do {
            if (requestShutdown) {
                _shutdown();
            } else if (ready < 0 && sockErr() != EINTR) {
                _err(_SELECT_ERR, ERR, true, -1);
            } else if (updateConfig) {
                printf("Updating config...\n");
                if (readConfig(CONFIG_FILE, &options) != 0) {
                    printf(WARN " - " _CONFIG_ERR);
                } else if (options.port != htons(serverAddr.sin_port)) {
                    server = prepareServer(server, options, &serverAddr);
                }
                updateConfig = false;
            }
            printf("Listening on port %i\n", options.port);
            FD_ZERO(&incomingConnections);
            FD_SET(server, &incomingConnections);
            FD_SET(awakeSelect, &incomingConnections);
        } while ((ready = select(server + 1, &incomingConnections, NULL, NULL, NULL)) < 0 || !FD_ISSET(server, &incomingConnections));
        printf("Incoming connection, port %d\n", htons(serverAddr.sin_port));
        addrLen = sizeof(clientAddr);
        _socket client = accept(server, (struct sockaddr*)&clientAddr, &addrLen);
        if (client < 0) {
            printf(WARN "Error serving client\n");
            continue;
        }
        if (options.multiProcess) {
            serveProc(client);
            closeSocket(client);
        } else {
            _socket* sock;
            if ((sock = malloc(sizeof(_socket))) == NULL) {
                printf("Error serving client: " _ALLOC_ERR);
                continue;
            }
            *sock = client;
            serveThread(sock);
        }
    }
}
