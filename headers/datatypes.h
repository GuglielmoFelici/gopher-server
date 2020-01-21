#ifndef DATATYPES_H
#define DATATYPES_H

#if defined(_WIN32)
#include <windows.h>
#define EINTR WSAEINTR
#define MAX_NAME MAX_PATH
#define PIPE_BUF 4096
typedef SOCKET _socket;
typedef LPSTR _string;
typedef LPCSTR _cstring;
typedef BOOL _handlerRet;
typedef HANDLE _thread, myPipe, _event, _map, _dir, _mutex, _cond;
typedef DWORD _procId, _sig_atomic;

#else

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#define MAX_NAME FILENAME_MAX
typedef int _socket, _event;
typedef int myPipe;
typedef char* _string;
typedef const char* _cstring;
typedef void _handlerRet;
typedef pthread_t _thread;
typedef void* (*LPTHREAD_START_ROUTINE)(void*);
typedef pid_t _procId;
typedef __sig_atomic_t _sig_atomic;
typedef DIR* _dir;
typedef pthread_mutex_t _mutex;
typedef pthread_cond_t _cond;

#endif

#endif