#include "includes/config.h"
#include "includes/datatypes.h"
#include "includes/everything.h"
#include "includes/log.h"
#include "includes/platform.h"

void gopher(_socket s, _string request) {
    _string response;
}

int main(int argc, _string* argv) {
    struct config options;
    _socket server;
    struct sockaddr_in address;
    int _errno;
    fd_set incomingConnections;
    int selectRet;

    if ((_errno = startup()) != 0) {
        err(_WINSOCK_ERROR, ERR, false, _errno);
    }
    /* Configuration */
    if ((_errno = initLog()) != 0) {
        err(_LOG_ERROR, ERR, true, _errno);
    }
    initConfig(&options);
    if (readConfig(CONFIG_FILE, &options) != 0) {
        _log(_CONFIG_ERROR, ERR, true);
    }
    if ((server = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        err(_SOCKET_ERROR, ERR, true, server);
    }
    _log(_SOCKET_OPEN, INFO, false);
    char enable = 1;
    if (setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        _log(_REUSE_ERROR, ERR, true);
    }
    if (setNonblocking(server) < 0) {
        err(_NONBLOCK_ERR, ERR, true, -1);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(options.port);
    if (bind(server, (struct sockaddr*)&address, sizeof(address)) < 0) {
        err(_BIND_ERROR, ERR, true, -1);
    }
    if (listen(server, 5) < 0) {
        err(_LISTEN_ERROR, ERR, true, -1);
    }
    _log(_SOCKET_LISTENING, INFO, false);

    while (true) {
        FD_ZERO(&incomingConnections);
        FD_SET(server, &incomingConnections);
        if (selectRet = select(server + 1, &incomingConnections, NULL, NULL, NULL) < 0) {
            /*
        TODO sighup handling
        if (sockErr() == EINTR) {
        }
        */
            if (selectRet == -1) {
                err(_SELECT_ERR, ERR, true, -1);
            }
        } else {
            // TODO thread accept
            struct sockaddr_in clientAddr;
            int addrLen = sizeof(clientAddr);
            printf("incoming connection");
            _socket client = accept(server, (struct sockaddr*)&clientAddr, &addrLen);
        }
    }

    printf("All done.\n");
    return 0;
}
