/** \file globals.h
 * Global configurations.
 */

#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdbool.h>

#include "../headers/datatypes.h"

#if defined(_WIN32)
#define EINTR WSAEINTR
#define MAX_NAME MAX_PATH
#else
#define MAX_NAME FILENAME_MAX
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

#define LOG_MUTEX_NAME "logMutex"
#define LOGGER_EVENT_NAME "logEvent"
#define LOG_FILE "logFile"
#define LOGGER_PATH "winLogger.exe"
#define HELPER_PATH "winGopherProcess.exe"
#define DEFAULT_MULTI_PROCESS 0
#define DEFAULT_PORT 7070

#define CONFIG_DEFAULT "config"
#define CONFIG_PORT_KEY "port"
#define CONFIG_MP_KEY "multiprocess"
#define CONFIG_SILENT_KEY "silent"
#define CONFIG_YES "yes"

/** Turn off with option -s (silent) to disable server logging. */
extern bool enableLogging;

/** The path to the installation directory */
extern char installDir[MAX_NAME];

/** The path to the configuration file */
extern char configPath[MAX_NAME];

#endif