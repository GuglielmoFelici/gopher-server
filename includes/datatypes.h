#ifndef DATATYPES_H
#define DATATYPES_H

#if defined(_WIN32)
#include <windows.h>

typedef SOCKET _socket;
typedef LPSTR _string;
typedef WIN32_FIND_DATA _fileData;

#else

#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types>

typedef int _socket;
typedef char* _string;
typedef struct stat _fileData;

#endif

#endif