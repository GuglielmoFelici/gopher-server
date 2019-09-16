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
#include <unistd.h>
#endif

_string errorString();

int startup();

int setNonblocking(_socket s);

bool isDirectory(_string path);

char gopherType(_string type);

void readDirectory(_string path, _string response);

char parseFileName(_string file);

#endif