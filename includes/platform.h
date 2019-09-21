#ifndef PLATFORM_H
#define PLATFORM_H

#include "config.h"
#include "datatypes.h"
#include "everything.h"
#include "log.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

/* Utils */
_string errorString();

int startup();

bool isDirectory(_fileData* file);

int sockerr();

// int setNonblocking(_socket s);

/* Gopher */

char gopherType(const _file* file);

void gopherResponse(_cstring path, _string response);

/* Threads */

struct threadArgs {
    _socket socket;
    struct config options;
};

void task(const struct threadArgs* args);

void serve(_socket socket, const struct config options);

#endif