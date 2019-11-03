#include "includes/datatypes.h"
#include "includes/gopher.h"
#include "includes/log.h"

_pipe logPipe;
struct sockaddr_in wakeAddr;
_socket wakeSelect;
_event logEvent;
_procId logger;
bool signaled = false;

_socket prepareServer(_socket server, const struct config options, struct sockaddr_in* address, bool reload) {
    if (reload) {
        if (closeSocket(server) < 0) {
            err(_CLOSE_SOCKET_ERR, ERR, true, -1);
        };
    }
    if ((server = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        err(_SOCKET_ERR, ERR, true, server);
    }
    char enable = 1;
    if (setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        _log(_REUSE_ERR, ERR, true);
    }
    address->sin_family = AF_INET;
    address->sin_addr.s_addr = INADDR_ANY;
    address->sin_port = htons(options.port);
    if (bind(server, (struct sockaddr*)address, sizeof(*address)) < 0) {
        err(_BIND_ERR, ERR, true, -1);
    }
    if (listen(server, 5) < 0) {
        err(_LISTEN_ERR, ERR, true, -1);
    }
    return server;
}

int main(int argc, _string* argv) {
    struct config options;
    _socket server;
    struct sockaddr_in serverAddr;
    struct sockaddr_in clientAddr;
    fd_set incomingConnections;
    int addrLen;
    int _errno;
    int ready;

    /* Configuration */

    // if ((_errno = startup()) != 0) {
    //     err(_STARTUP_ERR, ERR, false, _errno);
    // }
    installSigHandler();
    setvbuf(stdout, NULL, _IONBF, 0);
    if ((_errno = initLog()) != 0) {
        err(_LOG_ERR, ERR, true, _errno);
    }
    initConfig(&options);
    if (readConfig(CONFIG_FILE, &options) != 0) {
        _log(_CONFIG_ERR, ERR, true);
    }
    server = prepareServer(server, options, &serverAddr, false);
    _log(_SOCKET_OPEN, INFO, false);
    _log(_SOCKET_LISTENING, INFO, false);
    startTransferLog();
    printf("listening on port %i\n", options.port);

    /* Main loop*/

    ready = 0;
    while (true) {
        do {
            if (ready < 0 && sockErr() != EINTR) {
                err(_SELECT_ERR, ERR, true, -1);
            }
            if (signaled) {
                printf("Reading config...\n");
                if (readConfig(CONFIG_FILE, &options) != 0) {
                    _log(_CONFIG_ERR, ERR, true);
                }
                if (options.port != htons(serverAddr.sin_port)) {
                    server = prepareServer(server, options, &serverAddr, true);
                }
                signaled = false;
            }
            FD_ZERO(&incomingConnections);
            FD_SET(server, &incomingConnections);
            FD_SET(wakeSelect, &incomingConnections);
        } while ((ready = select(server + 1, &incomingConnections, NULL, NULL, NULL)) < 0 || !FD_ISSET(server, &incomingConnections));
        printf("incoming connection at port %d\n", htons(serverAddr.sin_port));
        addrLen = sizeof(clientAddr);
        _socket client = accept(server, (struct sockaddr*)&clientAddr, &addrLen);
        printf("serving...\n");
        if (options.multiProcess) {
            serveProc(client);
            closeSocket(client);
        } else {
            _socket* sock;
            if ((sock = malloc(sizeof(_socket))) == NULL) {
                _log(_ALLOC_ERR, ERR, true);
            } else {
                *sock = client;
                serveThread(sock);
            }
        }
    }

    printf("All done.\n");
    return 0;
}
