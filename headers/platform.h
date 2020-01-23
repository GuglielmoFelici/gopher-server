#ifndef PLATFORM_H
#define PLATFORM_H

#if defined(_WIN32)
#include <windows.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

#include <stdbool.h>
#include <stdlib.h>
#include "datatypes.h"

#define PLATFORM_SUCCESS 0
#define PLATFORM_FAILURE -1
#define MAX_ERROR_SIZE 60  // TODO
#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif

/* Utils */
bool endsWith(char* str1, char* str2);

int getCwd(char* dst, size_t size);

int changeCwd(const char* path);

/* Sockets */

int sockErr();

int closeSocket(socket_t s);

int sendAll(socket_t s, char* data, int length);

const char* inetNtoa(struct in_addr* addr, void* dst, size_t size);

/* Files */

int isFile(char* path);

int isDir(const char* path);

int iterateDir(const char* path, _dir* dir, char* name, size_t nameSize);

int closeDir(_dir dir);

typedef struct {
    void* view;
    int size;
} file_mapping_t;

int getFileMap(char* path, file_mapping_t* mapData);

int unmapMem(void* addr, size_t len);

/* Threads & processes */

int startThread(thread_t* tid, LPTHREAD_START_ROUTINE routine, void* args);

int detachThread(thread_t tid);

int daemonize();

#endif