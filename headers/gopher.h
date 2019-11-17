#ifndef GOPHER_H
#define GOPHER_H

#if defined(_WIN32)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#include <windows.h>
#else
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "datatypes.h"
#include "log.h"

_socket server;
_pipe logPipe;
bool updateConfig;
bool requestShutdown;
struct sockaddr_in awakeAddr;
_socket awakeSelect;
_procId loggerId;
_event logEvent;

/* Utils */
void errorString(char* error, size_t size);

void _err(_cstring message, _cstring level, bool stderror, int code);

_string trimEnding(_string str);

/* Config */

#define DEFAULT_PORT 70
#define DEFAULT_MULTI_PROCESS 0
#define CONFIG_FILE "config"

struct config {
    unsigned short port;
    int multiProcess;
};

void defaultConfig(struct config* options, int which);

#define READ_PORT 1
#define READ_MULTIPROCESS 2
#define READ_BOTH 3
int readConfig(const _string configPath, struct config* options, int which);

int startup();

void _shutdown();

/* Signals */

void installSigHandler();

/* Sockets */

int sockErr();

int closeSocket(_socket s);

/* Multi */

void serveThread(_socket* socket);

void serveProc(_socket socket);

void closeThread();

/* Gopher */

_thread gopher(_cstring selector, _socket sock);

void errorRoutine(void* sock);

struct sendFileArgs {
    void* src;
    size_t size;
    _socket dest;
    char name[MAX_NAME];
};

void* sendFile(void* sendFileArgs);

/* Transfer Log */

void startTransferLog();

void logTransfer();

#endif