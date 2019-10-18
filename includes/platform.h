#ifndef PLATFORM_H
#define PLATFORM_H

#if defined(_WIN32)
#include <WinSock2.h>
#include <windows.h>
#else
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "config.h"
#include "datatypes.h"
#include "gopher.h"
#include "log.h"
#include "stdlib.h"

extern bool sig;
extern _socket wakeSelect;

/* Utils */
void errorString(char* error);

int startup();

void startLogger();

/* Signals */

void installSigHandler();

/* Sockets */

int sockErr();

int closeSocket(_socket s);

// int setNonblocking(_socket s);

/* Thread */

void serve(_socket socket, bool multiProcess);

void closeThread();

#endif