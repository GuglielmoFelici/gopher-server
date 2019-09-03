#include "common.h"

void err(char* message, int code) {
    printf(message);
    exit(code);
}

void initConfig(struct config *options) {
    options->port = DEFAULT_PORT;
    options->multiProcess = DEFAULT_MULTI_PROCESS;
}

void readConfig(char* configFile, struct config *options) {
    
}