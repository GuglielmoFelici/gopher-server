#ifndef DATATYPES_H
#define DATATYPES_H

#if defined(_WIN32)
#include <windows.h>

typedef SOCKET _socket;
typedef LPSTR _string;
typedef LPCSTR _cstring;
typedef WIN32_FIND_DATA _fileData;

#else

#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef int _socket;
typedef char* _string;
typedef const char* _cstring;
typedef char _fileData;  // Vedi platform -> isDirectory, versione linux

#endif

#endif