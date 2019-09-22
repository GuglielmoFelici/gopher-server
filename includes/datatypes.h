#ifndef DATATYPES_H
#define DATATYPES_H

#if defined(_WIN32)
#include <windows.h>

#define EINTR WSAEINTR

typedef SOCKET _socket;
typedef LPSTR _string;
typedef LPCSTR _cstring;
typedef WIN32_FIND_DATA _fileData;
typedef struct _file {
    char name[MAX_PATH];
    char path[MAX_PATH];
    char filePath[MAX_PATH];
} _file;
typedef BOOL _handlerRet;

#else

#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef int _socket;
typedef char* _string;
typedef const char* _cstring;
typedef struct stat _fileData;
typedef struct _file {
    char name[FILENAME_MAX];
    char path[FILENAME_MAX];
    char filePath[FILENAME_MAX];
} _file;
typedef void _handlerRet;

#endif

#endif