#ifndef SERVER_H
#define SERVER_H

#if defined(_WIN32)
#else
#include <netinet/in.h>
#endif

#include <stdbool.h>
#include "datatypes.h"
#include "logger.h"

#define CONFIG_DELIMITER '='
#define DEFAULT_MULTI_PROCESS false
#define DEFAULT_PORT 7070
#define READ_PORT 0x0001
#define READ_MULTIPROCESS 0x0002
#define SERVER_UPDATE 0
#define SERVER_INIT -1
// Error codes
#define SERVER_SUCCESS 0
#define SERVER_FAILURE -1

typedef struct {
    socket_t sock;
    struct sockaddr_in sockAddr;
    unsigned short port;
    bool multiProcess;
    char configFile[MAX_NAME];
    char installationDir[MAX_NAME];
} server_t;

typedef struct {
    socket_t sock;
    int port;
    const logger_t* pLogger;
} server_thread_args_t;

int initServer(server_t* pServer);

int destroyServer(server_t* pServer);

int installDefaultSigHandlers();

int prepareSocket(server_t* pServer, int flags);

void defaultConfig(server_t* pServer, int which);

int readConfig(server_t* pServer, int which);

void printHeading(const server_t* pServer);

int runServer(server_t* pServer, logger_t* pLogger);

#endif