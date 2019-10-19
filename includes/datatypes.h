#ifndef DATATYPES_H
#define DATATYPES_H

#if defined(_WIN32)
#include <windows.h>
#define EINTR WSAEINTR
#define MAX_NAME MAX_PATH / 2
typedef SOCKET _socket;
typedef LPSTR _string;
typedef LPCSTR _cstring;
typedef WIN32_FIND_DATA _fileData;
typedef BOOL _handlerRet;
typedef HANDLE _thread;
typedef HANDLE _pipe;

#else

#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#define MAX_NAME FILENAME_MAX / 2
typedef int _socket;
typedef char* _string;
typedef const char* _cstring;
typedef struct stat _fileData;
typedef void _handlerRet;
typedef pthread_t _thread;
typedef int _pipe;

#endif

typedef struct _file {
    char name[MAX_NAME];
    char path[MAX_NAME];
} _file;

#endif