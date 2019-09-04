#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_PORT 70
#define DEFAULT_MULTI_PROCESS false

struct config {
    unsigned short port;
    bool multiProcess;
};

void err(const char* message, int code);

void initConfig(struct config *options);

bool readConfig(const char *configPath, struct config *options);