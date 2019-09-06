#ifndef PLATFORM_H
#define PLATFORM_H

#include "everything.h"
#include "datatypes.h"
#include "log.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#include <sys/socket>
#endif

_string errorString();

int startup();

int setNonblocking(_socket s);

void readDirectory(_string path, _string response);

char gopherType(_string type);

#endif