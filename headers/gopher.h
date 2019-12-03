#ifndef GOPHER_H
#define GOPHER_H

#if defined(_WIN32)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#include <fcntl.h>
#include <io.h>
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
myPipe logPipe;
_sig_atomic updateConfig;
_sig_atomic requestShutdown;
struct sockaddr_in awakeAddr;
_socket awakeSelect;
_procId loggerPid;
_procId serverPid;
_event logEvent;
char installationDir[MAX_NAME];

/* Utils */

void _shutdown();

void errorString(char* error, size_t size);

void _err(_cstring message, _cstring level, bool stderror, int code);

void _logErr(_cstring message);

_string trimEnding(_string str);

void changeCwd(_cstring path);

/* Config */

#define DEFAULT_PORT 7070
#define DEFAULT_MULTI_PROCESS 0
#define CONFIG_FILE "config"

struct config {
    unsigned short port;
    int multiProcess;
};

void defaultConfig(struct config* options, int which);

#define READ_PORT 0
#define READ_MULTIPROCESS 1
#define READ_BOTH 2
int readConfig(struct config* options, int which);

int startup();

/* Signals */

void installDefaultSigHandlers();

/* Sockets */

int sockErr();

int closeSocket(_socket s);

/* Threads & processes */

void serveThread(_socket* socket);

void serveProc(_socket socket);

void closeThread();

/* Gopher */

_thread gopher(_socket sock);

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