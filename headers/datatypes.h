#ifndef DATATYPES_H
#define DATATYPES_H

#if defined(_WIN32)
#include <windows.h>
#define EINTR WSAEINTR
#define MAX_NAME MAX_PATH
#define PIPE_BUF 4096
typedef SOCKET socket_t;
typedef LPSTR _string;
typedef LPCSTR _cstring;
typedef BOOL _handlerRet;
typedef HANDLE thread_t, pipe_t, event_t, _map, _dir, mutex_t, cond_t;
typedef DWORD proc_id_t, _sig_atomic;

#else

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#define MAX_NAME FILENAME_MAX
typedef int socket_t, event_t;
typedef int pipe_t;
typedef char* _string;
typedef const char* _cstring;
typedef void _handlerRet;
typedef pthread_t thread_t;
typedef void* (*LPTHREAD_START_ROUTINE)(void*);
typedef __sig_atomic_t _sig_atomic;
typedef DIR* _dir;
typedef pthread_mutex_t mutex_t;
typedef pthread_cond_t cond_t;
typedef pid_t proc_id_t;

#endif

#endif