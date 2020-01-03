#ifndef LOG_H
#define LOG_H

#define _GNU_SOURCE
#include "datatypes.h"
#include "gopher.h"

#define _CONFIG_ERR "Can't read configuration file, default settings will be used."
#define _MULTIPROCESS_CONFIG_ERR "Can't read configuration file, multiprocess will be disabled."
#define _PORT_CONFIG_ERR "Can't read configuration for port, default one will be used."
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
#define USAGE "Available options:\n-h\tshow usage help\n-p=PORT\tsets the port to PORT\n-d=DIR\tchanges cwd of the server to DIR\n-m\tactivates multiprocess mode"

#define WARN "WARNING"
#define ERR "ERROR"

#endif