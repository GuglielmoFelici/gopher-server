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

_socket openSocket(int domain, int type, int protocol);

int bindSocket(_socket s, const struct sockaddr *addr, int namelen);

int setNonblocking(_socket s);

#endif