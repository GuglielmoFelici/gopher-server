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

bool isDirectory(const _fileData* file);

char gopherType(char* file);

void readDirectory(const _string path, _string response);

#endif