#ifndef PLATFORM_H
#define PLATFORM_H

#if defined(_WIN32)
#include <windows.h>
#define LOG_DEBUG 0
#define LOG_INFO 1
#define LOG_WARNING 2
#define LOG_ERR 3

#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <syslog.h>
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

#include <stdbool.h>
#include <stdlib.h>
#include "datatypes.h"

#define PLATFORM_SUCCESS 0x0000
#define PLATFORM_FAILURE 0x0001
#define PLATFORM_FILE_ERR 0x0001
#define PLATFORM_ISFILE 0x0002
#define PLATFORM_ISDIR 0x0004
#define PLATFORM_NOT_FOUND 0x0008
#define PLATFORM_END_OF_DIR 0x0010

/**
 * Returns true if the string str2 ends with the string str2.
 * @param str1 The string to check.
 * @return LOGGER_SUCCESS or LOGGER_FAILURE.
 * @see logger_t
 */
bool endsWith(cstring_t str1, cstring_t str2);

int getCwd(string_t dst, size_t size);

int changeCwd(cstring_t path);

void logMessage(cstring_t message, int level);

/* Sockets */

int sockErr();

int closeSocket(socket_t s);

int sendAll(socket_t s, cstring_t data, int length);

cstring_t inetNtoa(const struct in_addr* addr, void* dst, size_t size);

/* Files */

int fileAttributes(cstring_t path);

int getFileSize(cstring_t path);

int iterateDir(cstring_t path, _dir* dir, string_t name, size_t nameSize);

int closeDir(_dir dir);

typedef struct {
    void* view;
    int size;
} file_mapping_t;

int getFileMap(cstring_t path, file_mapping_t* mapData);

int unmapMem(void* addr, size_t len);

/* Threads & processes */

int startThread(thread_t* tid, LPTHREAD_START_ROUTINE routine, void* args);

int detachThread(thread_t tid);

int daemonize();

void debugPause(cstring_t message);

#endif