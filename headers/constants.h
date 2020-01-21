#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <math.h>

#if defined(_WIN32)  // WINDOWS

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#define HELPER_PATH "helpers\\winGopherProcess.exe"
#define LOGGER_PATH "helpers\\winLogger.exe"
#define LOGGER_EVENT_NAME "logEvent"
#define DIR_SEP "\\"

#else  // LINUX

#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define DIR_SEP "/"

#endif

#define MAX_ERROR_SIZE 60
#define IP_ADDRESS_LENGTH 16

/* Config */

#define CONFIG_DELIMITER '='
#define CONFIG_FILE "config"
#define DEFAULT_MULTI_PROCESS 0
#define DEFAULT_PORT 7070
#define INVALID_PORT 0
#define INVALID_MULTIPROCESS -1
#define READ_PORT 0x0001
#define READ_MULTIPROCESS 0x0002
#define SERVER_INIT -1

/* Logging */
#define LOG_MUTEX_NAME "logMutex"

/* Gopher */

#define CRLF "\r\n"
#define BUFF_SIZE 70
#define GOPHER_SUCCESS 0x0000
#define GOPHER_FAILURE 0x0001
#define GOPHER_NOT_FOUND 0x0002
#define GOPHER_END_OF_DIR 0x0004
#define ERROR_MSG "3Error"
#define BAD_SELECTOR_MSG "Bad selector"
#define RESOURCE_NOT_FOUND_MSG "Resource not found"
#define SYS_ERR_MSG " Internal error"
#define GOPHER_DOMAIN "localhost"
// Gopher types
#define GOPHER_UNKNOWN 'X'
#define GOPHER_TEXT '0'
#define GOPHER_DIR '1'
#define GOPHER_CCSO '2'
#define GOPHER_ERR '3'
#define GOPHER_BINHEX '4'
#define GOPHER_DOS '5'
#define GOPHER_UUENC '6'
#define GOPHER_FULLTXT '7'
#define GOPHER_TELNET '8'
#define GOPHER_BINARY '9'
#define GOPHER_GIF 'g'
#define GOPHER_IMAGE 'I'

#define EXT_NO 16
static char* extensions[EXT_NO] = {".txt", ".doc", ".odt", ".rtf",
                                   ".c", ".cpp", ".h", ".bat",
                                   ".sh", ".hqx", ".dos", ".exe",
                                   ".gif", ".jpg", ".jpeg", ".png"};
// Extension groups (bits set to 1 are the indexes in the array of extensions that belong to the group)
#define EXT_TXT 0X01FF
#define EXT_HQX 0x0200
#define EXT_DOS 0x0400
#define EXT_BIN 0x0800
#define EXT_GIF 0x1000
#define EXT_IMG 0xE000
#define CHECK_GRP(index, group) group&(int)pow(2., index)

#define FILE_CMD_MAX 256
#define FILE_CMD_NOT_FOUND "No such file or directory"
#define FILE_CMD_EXEC1 "executable"
#define FILE_CMD_EXEC2 "ELF"
#define FILE_CMD_IMG "image"
#define FILE_CMD_DIR "directory"
#define FILE_CMD_GIF "GIF"
#define FILE_CMD_TXT "text"

#endif