#ifndef CONSTANTS_H
#define CONSTANTS_H

#if defined(_WIN32)

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#define CRLF "\r\n"
#define HELPER_PATH "helpers\\winGopherProcess.exe"

#else

#define CRLF "\n"
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1

#endif

/* Config */

#define CONFIG_DELIMITER '='
#define CONFIG_FILE "config"
#define DEFAULT_MULTI_PROCESS 0
#define DEFAULT_PORT 7070
#define INVALID_PORT 0
#define INVALID_MULTIPROCESS -1
#define READ_PORT 1
#define READ_MULTIPROCESS 2
#define SERVER_INIT -1

/* Gopher */

#define BUFF_SIZE 70
#define GOPHER_ERROR_MSG "3Error retrieving the resource"
#define GOHPER_BAD_SELECTOR "Bad selector"
#define GOPHER_SUCCESS 0
#define GOPHER_FAILURE -1

#endif