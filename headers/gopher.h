#ifndef GOPHER_H
#define GOPHER_H

#if defined(_WIN32)

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
#include "constants.h"

#include "datatypes.h"
#include "log.h"

_socket awakeSelect;
struct sockaddr_in awakeAddr;
_sig_atomic volatile updateConfig;
_sig_atomic volatile requestShutdown;
_mutex* logMutex;
_cond* logCond;
myPipe logPipe;
_procId loggerPid;
_event logEvent;
char installationDir[MAX_NAME];

/* Utils */

void _shutdown();

void errorString(char* error, size_t size);

void _err(_cstring message, bool stderror, int code);

void _logErr(_cstring message);

bool endsWith(char* str1, char* str2);

_string trimEnding(_string str);

int getCwd(_string dst, size_t size);

void changeCwd(_cstring path);

struct config {
    unsigned short port;
    int multiProcess;
};

void defaultConfig(struct config* options, int which);

int readConfig(struct config* options, int which);

int startup();

void printHeading(struct config* options);

/* Signals */

void installDefaultSigHandlers();

/* Sockets */

int sockErr();

int closeSocket(_socket s);

int sendAll(_socket s, char* data, int length);

const char* inetNtoa(struct in_addr* addr, void* dst, size_t size);

/* Threads & processes */

struct threadArgs {
    _socket sock;
    unsigned short port;
};

int _createThread(_thread* tid, LPTHREAD_START_ROUTINE routine, void* args);

int serveThread(_socket socket, unsigned short port);

int serveProc(_socket socket, unsigned short port);

bool detachThread(_thread tid);

/* Files */

int isFile(_string path);

int isDir(const char* path);

int iterateDir(_cstring path, _dir* dir, _string name, size_t nameSize);

int closeDir(_dir dir);

struct fileMappingData {
    void* view;
    int size;
};

int getFileMap(_string path, struct fileMappingData* mapData);

int unmapMem(void* addr, size_t len);

/* Gopher */

int sendErrorResponse(_socket sock, _string msg);

int gopher(_socket sock, unsigned short port);

void errorRoutine(void* sock);

struct sendFileArgs {
    void* src;
    int size;
    _socket dest;
    char name[MAX_NAME];
};

/* Transfer Log */

int startTransferLog();

int logTransfer(_string log);

#endif