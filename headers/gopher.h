#ifndef GOPHER_H
#define GOPHER_H

#if defined(_WIN32)

#include <fcntl.h>
#include <io.h>
#include <windows.h>

#else
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "constants.h"

#include "datatypes.h"
#include "log.h"

/* Utils */

void _shutdown();

void errorString(char* error, size_t size);

void _err(_cstring message, bool stderror, int code);

void _logErr(_cstring message);

#endif