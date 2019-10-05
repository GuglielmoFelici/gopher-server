#include "includes/config.h"
#include "includes/datatypes.h"
#include "includes/everything.h"
#include "includes/log.h"
#include "includes/platform.h"

_socket prepareServer(_socket server, const struct config options, struct sockaddr_in* address, bool reload) {
    char enable = 1;
    if (reload) {
        if (closeSocket(server) < 0) {
            err(_CLOSE_SOCKET_ERR, ERR, true, -1);
        };
    }
    if ((server = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        err(_SOCKET_ERROR, ERR, true, server);
    }
    if (setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        _log(_REUSE_ERROR, ERR, true);
    }
    address->sin_family = AF_INET;
    address->sin_addr.s_addr = INADDR_ANY;
    address->sin_port = htons(options.port);
    if (bind(server, (struct sockaddr*)address, sizeof(*address)) < 0) {
        err(_BIND_ERROR, ERR, true, -1);
    }
    if (listen(server, 5) < 0) {
        err(_LISTEN_ERROR, ERR, true, -1);
    }
    return server;
}

int main(int argc, _string* argv) {
    struct config options;
    struct sockaddr_in address;
    _socket server;
    int _errno;
    int selResult;
    int ready = 0;
    fd_set incomingConnections;

    /* Configuration */

    if ((_errno = startup()) != 0) {
        err(_WINSOCK_ERROR, ERR, false, _errno);
    }
    installSigHandler();
    setvbuf(stdout, NULL, _IONBF, 0);
    if ((_errno = initLog()) != 0) {
        err(_LOG_ERROR, ERR, true, _errno);
    }
    initConfig(&options);
    if (readConfig(CONFIG_FILE, &options) != 0) {
        _log(_CONFIG_ERROR, ERR, true);
    }
    server = prepareServer(server, options, &address, false);
    _log(_SOCKET_OPEN, INFO, false);
    _log(_SOCKET_LISTENING, INFO, false);

    /* Main loop*/

    while (true) {
        do {
            if (ready < 0 && sockErr() != EINTR) {
                err(_SELECT_ERR, ERR, true, -1);
            }
            if (sig) {
                printf("Reading config...\n");
                if (readConfig(CONFIG_FILE, &options) != 0) {
                    _log(_CONFIG_ERROR, ERR, true);
                }
                if (options.port != htons(address.sin_port)) {
                    server = prepareServer(server, options, &address, true);
                }
                sig = false;
            }
            FD_ZERO(&incomingConnections);
            FD_SET(server, &incomingConnections);
            FD_SET(wakeSelect, &incomingConnections);
        } while ((ready = select(server + 1, &incomingConnections, NULL, NULL, NULL)) < 0 || !FD_ISSET(server, &incomingConnections));
        // TODO thread accept
        printf("incoming connection at port %d\n", htons(address.sin_port));
        struct sockaddr_in clientAddr;
        int addrLen = sizeof(clientAddr);
        _socket client = accept(server, (struct sockaddr*)&clientAddr, &addrLen);
        setsockopt(client, SOL_SOCKET, SO_DONTLINGER, "0", 1);
        serve(client, options.multiProcess);
    }

    printf("All done.\n");
    return 0;
}
