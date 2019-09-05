#ifndef DATATYPES_H
#define DATATYPES_H

#if defined(_WIN32)
#include <windows.h>

typedef SOCKET _socket;
typedef LPTSTR _string;
typedef BOOL _bool;

#else

typedef int _socket;
typedef char* _string;
typedef bool _bool;

#endif

#endif