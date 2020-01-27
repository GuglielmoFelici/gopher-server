#ifndef LOG_H
#define LOG_H

#define _GNU_SOURCE
#include "datatypes.h"

#define MAIN_CONFIG_ERR "Can't read configuration file, default settings will be used."
#define MAIN_MULTIPROCESS_CONFIG_ERR "Can't read configuration file, multiprocess will be disabled."
#define MAIN_PORT_CONFIG_ERR "Can't read configuration for port, default one will be used."
#define MAIN_CWD_ERR "Can't get current working directory"
#define MAIN_STARTUP_ERR "Errore starting up."
#define MAIN_SELECT_ERR "Select failed."
#define MAIN_SOCKET_ERR "Can't initialize socket."
#define MAIN_BIND_ERR "Can't bind socket."
#define MAIN_LISTEN_ERR "Socket - listen failed."
#define MAIN_START_LOG_ERR "Error starting logger."
#define MAIN_CLOSE_LOG_ERR "Error stopping logger."
#define MAIN_FORK_ERR "Fork failed."
#define MAIN_DAEMON_ERR "Can't daemonize process."
#define MAIN_THREAD_ERR "Error starting a new thread."
#define MAIN_CLOSE_SOCKET_ERR "Errore closing socket."
#define MAIN_ALLOC_ERR "Memory allocation failed."
#define MAIN_SYS_ERR "System error."
#define MAIN_USAGE "Available options:\n-h\tshow usage help\n-p=PORT\tsets the port to PORT\n-d=DIR\tchanges cwd of the server to DIR\n-m\tactivates multiprocess mode"

#define WARN "WARNING - "

#endif