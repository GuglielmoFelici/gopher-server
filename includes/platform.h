#ifndef PLATFORM_H
#define PLATFORM_H

#include "config.h"
#include "datatypes.h"
#include "everything.h"
#include "gopher.h"
#include "log.h"

#if defined(_WIN32)
#include <WinSock2.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

extern bool sig;
extern _socket wakeSelect;

/* Utils */
void errorString(char* error);

int startup();

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