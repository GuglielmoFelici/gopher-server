#ifndef DATATYPES_H
#define DATATYPES_H

#if defined(_WIN32)
#include <windows.h>

typedef SOCKET _socket;
typedef LPSTR _string;

#else

typedef int _socket;
typedef char* _string;
typedef bool _bool;

#endif

#endif