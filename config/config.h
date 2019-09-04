#include "../everything.h"

#define DEFAULT_PORT 70
#define DEFAULT_MULTI_PROCESS false

#define CONFIG_FILE "config/config"

struct config {
    unsigned short port;
    bool multiProcess;
};

void initConfig(struct config *options);

bool readConfig(const char *configPath, struct config *options);