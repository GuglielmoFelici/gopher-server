#ifndef CONFIG_H
#define CONFIG_H

#include "datatypes.h"
#include "stdlib.h"

#define DEFAULT_PORT 70
#define DEFAULT_MULTI_PROCESS false

#define CONFIG_FILE "config/config"

struct config {
    unsigned short port;
    bool multiProcess;
};

void initConfig(struct config *options);

bool readConfig(const _string configPath, struct config *options);

#endif