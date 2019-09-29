#include "../includes/config.h"

void initConfig(struct config *options) {
    options->port = DEFAULT_PORT;
    options->multiProcess = DEFAULT_MULTI_PROCESS;
}

bool readConfig(const _string configPath, struct config *options) {
    FILE *configFile;
    char port[6];
    char multiProcess[2];
    configFile = fopen(configPath, "r");
    if (configFile == NULL) {
        return errno;
    }
    while (fgetc(configFile) != '=')
        ;
    fgets(port, 6, configFile);
    while (fgetc(configFile) != '=')
        ;
    fgets(multiProcess, 2, configFile);
    options->port = atoi(port);
    options->multiProcess = (bool)atoi(multiProcess);
    fclose(configFile);
    return 0;
}