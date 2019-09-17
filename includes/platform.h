#ifndef PLATFORM_H
#define PLATFORM_H

#include "datatypes.h"
#include "everything.h"
#include "log.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

_string errorString();

int startup();

int setNonblocking(_socket s);

bool isDirectory(_fileData data);

char gopherType(_fileData data);

void readDirectory(_string path, _string response);

#endif