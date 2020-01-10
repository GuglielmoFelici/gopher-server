#ifndef DATATYPES_H
#define DATATYPES_H

#if defined(_WIN32)
#include <windows.h>
#define EINTR WSAEINTR
#define MAX_NAME MAX_PATH / 2
#define PIPE_BUF 4096
typedef SOCKET _socket;
typedef LPSTR _string;
typedef LPCSTR _cstring;
typedef WIN32_FIND_DATA _fileData;
typedef BOOL _handlerRet;
typedef HANDLE _thread, myPipe, _event, _map;
typedef _LARGE_INTEGER _largeInt;
typedef DWORD _procId, _sig_atomic;

#else

#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#define MAX_NAME FILENAME_MAX / 2
typedef int _socket, _event;
typedef int myPipe;
typedef char* _string;
typedef const char* _cstring;
typedef struct stat _fileData;
typedef void _handlerRet;
typedef pthread_t _thread;
typedef pid_t _procId;
typedef __sig_atomic_t _sig_atomic;
typedef unsigned long long _largeInt;

#endif

#endif