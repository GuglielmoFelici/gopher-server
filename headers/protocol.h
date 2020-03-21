/** \file protocol.h 
 *  Executes the gopher protocol for a client.
 *  @file protocol.c
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "datatypes.h"
#include "logger.h"

#define CRLF "\r\n"
#define BUFF_SIZE 70
// Return codes
#define GOPHER_SUCCESS 0x0000
#define GOPHER_FAILURE 0x0001
#define GOPHER_NOT_FOUND 0x0002
// Messages
#define ERROR_MSG "3Error"
#define RESOURCE_NOT_FOUND_MSG "Resource not found"
#define INVALID_SELECTOR "Invalid selector"
#define SYS_ERR_MSG " Internal error"
#define GOPHER_DOMAIN "localhost"
// Types
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
// TODO
// Extension groups (bits set to 1 are the indexes in the array of extensions that belong to the group)
#define EXT_TXT 0X01FF
#define EXT_HQX 0x0200
#define EXT_DOS 0x0400
#define EXT_BIN 0x0800
#define EXT_GIF 0x1000
#define EXT_IMG 0xE000
#define CHECK_GRP(index, group) ((group) & (1 << (index)))

#define FILE_CMD_MAX 256
#define FILE_CMD_NOT_FOUND "No such file or directory"
#define FILE_CMD_EXEC1 "executable"
#define FILE_CMD_EXEC2 "ELF"
#define FILE_CMD_IMG "image"
#define FILE_CMD_DIR "directory"
#define FILE_CMD_GIF "GIF"
#define FILE_CMD_TXT "text"

int gopher(socket_t sock, int port, const logger_t* pLogger);

typedef struct {
    void* src;
    int size;
    socket_t dest;
    char name[MAX_NAME];
    const logger_t* pLogger;
} send_args_t;

#endif