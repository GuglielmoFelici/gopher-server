#include "includes/config.h"
#include "includes/datatypes.h"
#include "includes/everything.h"
#include "includes/log.h"
#include "includes/platform.h"


int main(int argc, char** argv) {

    _socket server;
    struct config options;
    int _errno;
    
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

    if (server = openSocket(AF_INET, SOCK_STREAM, 0) < 0) {
        err(_SOCKET_ERROR, ERR, true, server);
    }


}