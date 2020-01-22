#ifndef SERVER_H
#define SERVER_H

#include <stdbool.h>
#include "datatypes.h"
#include "logger.h"

#define CONFIG_DELIMITER '='
#define CONFIG_FILE "config"
#define DEFAULT_MULTI_PROCESS 0
#define DEFAULT_PORT 7070
#define INVALID_PORT 0
#define INVALID_MULTIPROCESS -1
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
    char installationDir[MAX_NAME];
} server_t;

struct threadArgs {
    socket_t sock;
    unsigned short port;
};  // TODO typedef

int initServer(server_t* pServer);

int installDefaultSigHandlers();

int prepareSocket(server_t* pServer, int flags);

void defaultConfig(server_t* pServer, int which);

int readConfig(server_t* pServer, int which);

void printHeading(const server_t* pServer);

int runServer(server_t* pServer);

int serveThread(socket_t socket, int port);

int serveProc(socket_t socket, logger_t* pLogger, server_t* pServer);

#endif