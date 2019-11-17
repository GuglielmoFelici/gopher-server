#ifndef LOG_H
#define LOG_H

#define _GNU_SOURCE
#include "datatypes.h"
#include "gopher.h"

#define _CONFIG_ERR "Can't read configuration file, default options will be used."
#define _STARTUP_ERR "Errore starting up."
#define _SELECT_ERR "Select failed."
#define _SOCKET_ERR "Can't initialize socket."
#define _BIND_ERR "Can't bind socket."
#define _LISTEN_ERR "Socket - listen failed."
#define _FORK_ERR "Fork failed."
#define _DAEMON_ERR "Can't daemonize process."
#define _THREAD_ERR "Error starting a new thread."
#define _CLOSE_SOCKET_ERR "Errore closing socket."
#define _ALLOC_ERR "Memory allocation failed."

#define WARN "WARNING"
#define ERR "ERROR"

#endif