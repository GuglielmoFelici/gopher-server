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

const char* extensions[] = {"txt", "doc", "odt", "rtf",
                            "c", "cpp", "java", "bat",
                            "hqx", "dos", "exe", "jar",
                            "bin", "gif", "jpg", "jpeg",
                            "png"};

#define BUFF_SIZE 70
#define GOPHER_SUCCESS 0
#define GOPHER_FAILURE -1
#define ERROR_MSG "3Error retrieving the resource"
#define BAD_SELECTOR_MSG "Bad selector"
#define FILE_NOT_FOUND_MSG "Can't find the resource"

#define GOPHER_UNKNOWN 'X'
#define GOPHER_TEXT '0'
#define GOPHER_BINHEX '4'
#define GOPHER_DOS '5'
#define GOPHER_BINARY '9'
#define GOPHER_GIF 'g'
#define GOPHER_IMAGE 'I'

// TODO
#define EXT_TEXT 0X00FF
#define EXT_HQX 0x0100
#define EXT_DOS 0x0200
#define EXT_BIN 0x1C00
#define EXT_GIF 0x2000
#define EXT_JPG 0x2000
#define EXT_JPEG 0x4000
#define EXT_JPG 0x8000
#define EXT_MAX 0x10000

#endif