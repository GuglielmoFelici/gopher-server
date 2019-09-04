#include "everything.h"
#include "datatypes.h"
#include "platform.h"
#include "errors.h"
#include "config/config.h"
#include "log/log.h"

#define CONFIG_FILE "config"

int main(int argc, char** argv) {

    _socket server;
    
    startup();
    /* Configuration */
    if (!initLog()) {
        err(LOG_ERROR, ERR, -1);
    }
    struct config options;
    initConfig(&options);
    if (!readConfig(CONFIG_FILE, &options)) {
        _log(CONFIG_ERROR, ERR);
    }

    server = openSocket(AF_INET, SOCK_STREAM, 0);


}