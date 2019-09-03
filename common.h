#include <stdbool.h>

#define DEFAULT_PORT 70
#define DEFAULT_MULTI_PROCESS false

struct config {
    unsigned short port;
    bool multiProcess;
};

void err(char* message, int code);

void initConfig(struct config *options);

void readConfig(char *configFile, struct config *options);