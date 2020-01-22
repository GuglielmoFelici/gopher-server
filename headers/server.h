#include <stdbool.h>
#include "datatypes.h"

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

int startup();

int installDefaultSigHandlers();

int prepareSocket(server_t* pServer, int flags);

void defaultConfig(server_t* pServer, int which);

int readConfig(server_t* pServer, int which);

int startServer(server_t* pServer);