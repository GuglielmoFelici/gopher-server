#include "common.h"

void err(const char* message, int code) {
    fprintf(stderr, message);
    exit(code);
}

void initConfig(struct config *options) {
    options->port = DEFAULT_PORT;
    options->multiProcess = DEFAULT_MULTI_PROCESS;
}

bool readConfig(const char* configPath, struct config *options) {
    FILE *configFile;
    char port[6];
    char multiProcess[2];
    configFile = fopen(configPath, "r");
    if (configFile == NULL) {
        perror(configPath);
        return false;
    }
    while (fgetc(configFile) != '=');
    fgets(port, 6, configFile);
    while (fgetc(configFile) != '=');
    fgets(multiProcess, 2, configFile);
    options->port = atoi(port);
    options->multiProcess = (bool)atoi(multiProcess);
    return true;
}