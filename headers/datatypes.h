#ifndef DATATYPES_H
#define DATATYPES_H

#if defined(_WIN32)
#include <windows.h>
#define EINTR WSAEINTR
// TODO perché fratto 2?
#define MAX_NAME MAX_PATH / 2
#define PIPE_BUF 4096
typedef SOCKET _socket;
typedef LPSTR _string;
typedef LPCSTR _cstring;
typedef WIN32_FIND_DATA _fileData;
typedef BOOL _handlerRet;
typedef HANDLE _thread, pipe, _event;
typedef DWORD _procId;

#else

#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#define MAX_NAME FILENAME_MAX / 2
typedef int _socket, _event;
typedef int pipe;
typedef char* _string;
typedef const char* _cstring;
typedef struct stat _fileData;
typedef void _handlerRet;
typedef pthread_t _thread;
typedef pid_t _procId;

#endif

#define MAX_GOPHER_MSG 256
#define MAX_SECONDS_WAIT 6

typedef struct _file {
    char name[MAX_NAME];
    char path[MAX_NAME];
} _file;

#endif