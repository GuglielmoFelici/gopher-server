#include "../headers/protocol.h"
#include <stdbool.h>
#include <stdio.h>
#include <windows.h>
#include "../headers/datatypes.h"
#include "../headers/logger.h"
#include "../headers/platform.h"

#define MAX_LINE 70

#if defined(_WIN32)

/*****************************************************************************************************************/
/*                                             WINDOWS FUNCTIONS                                                 */

/*****************************************************************************************************************/

static void normalizeInput(char* str) {
    char* strtokptr;
    if (strncmp(str, CRLF, sizeof(CRLF)) == 0) {
        strcpy(str, "./");
    } else {
        strtok_r(str, CRLF, &strtokptr);
    }
    for (int i = 0; i < strlen(str); i++) {
        if (str[i] == '\\') {
            str[i] = '/';
        }
    }
}

/* Ritorna il carattere di codifica del tipo di file */
char gopherType(LPSTR filePath) {
    LPSTR ext;
    if (isDir(filePath) == GOPHER_SUCCESS) {
        return GOPHER_DIR;
    }
    ext = strrchr(filePath, '.');
    if (!ext) {
        return GOPHER_UNKNOWN;
    }
    for (int i = 0; i < EXT_NO; i++) {
        if (strcmp(ext, extensions[i]) == 0) {
            if (CHECK_GRP(i, EXT_TXT)) {
                return GOPHER_TEXT;
            } else if (CHECK_GRP(i, EXT_HQX)) {
                return GOPHER_BINHEX;
            } else if (CHECK_GRP(i, EXT_DOS)) {
                return GOPHER_DOS;
            } else if (CHECK_GRP(i, EXT_BIN)) {
                return GOPHER_BINARY;
            } else if (CHECK_GRP(i, EXT_GIF)) {
                return GOPHER_GIF;
            } else if (CHECK_GRP(i, EXT_IMG)) {
                return GOPHER_IMAGE;
            }
        }
    }
    return GOPHER_UNKNOWN;
}

#else

/*****************************************************************************************************************/
/*                                             LINUX FUNCTIONS                                                    */

/*****************************************************************************************************************/

static void normalizeInput(char* str) {
    char* strtokptr;
    if (strncmp(str, CRLF, sizeof(CRLF)) == 0) {
        strcpy(str, "./");
    } else {
        strtok_r(str, CRLF, &strtokptr);
    }
}

/* Ritorna il carattere di codifica del tipo di file */
char gopherType(char* file) {
    char cmd[MAX_NAME];
    char cmdOut[FILE_CMD_MAX];
    FILE* response;
    int bytesRead;
    if (snprintf(cmd, sizeof(cmd), "file \"%s\"", file) < 0) {
        return GOPHER_UNKNOWN;
    }
    response = popen(cmd, "r");
    bytesRead = fread(cmdOut, 1, FILE_CMD_MAX, response);
    fclose(response);
    if (strstr(cmdOut, FILE_CMD_NOT_FOUND)) {
        return GOPHER_UNKNOWN;
    } else if (strstr(cmdOut, FILE_CMD_EXEC1) || strstr(cmdOut, FILE_CMD_EXEC2)) {
        return GOPHER_BINARY;
    } else if (strstr(cmdOut, FILE_CMD_IMG)) {
        return GOPHER_IMAGE;
    } else if (strstr(cmdOut, FILE_CMD_DIR)) {
        return GOPHER_DIR;
    } else if (strstr(cmdOut, FILE_CMD_GIF)) {
        return GOPHER_GIF;
    } else if (strstr(cmdOut, FILE_CMD_TXT)) {
        return GOPHER_TEXT;
    }
    return GOPHER_UNKNOWN;
}

#endif

/*****************************************************************************************************************/
/*                                             COMMON                                                            */

/*****************************************************************************************************************/

static bool validateInput(char* str) {
    char* strtokptr;
    char* ret;
    ret = strtok_r(str, CRLF, &strtokptr);
    return ret == NULL ||
           !strstr(ret, ".\\") && !strstr(ret, "./");
}

static int sendErrorResponse(socket_t sock, char* msg) {
    if (SOCKET_ERROR == sendAll(sock, ERROR_MSG " - ", sizeof(ERROR_MSG) + 3)) {
        return GOPHER_FAILURE;
    }
    if (SOCKET_ERROR == sendAll(sock, msg, strlen(msg))) {
        return GOPHER_FAILURE;
    }
    if (SOCKET_ERROR == sendAll(sock, CRLF ".", 3)) {
        return GOPHER_FAILURE;
    }
    if (SOCKET_ERROR == closeSocket(sock)) {
        return GOPHER_FAILURE;
    }
    return GOPHER_SUCCESS;
}

/* Costruisce la lista dei file e la invia al client */
static int sendDir(const char* path, int sock, int port) {
    _dir dir = NULL;
    char fileName[MAX_NAME], *filePath = NULL, *line = NULL;
    size_t lineSize, filePathSize;
    char type;
    int res;
    if ((res = isDir(path)) != GOPHER_SUCCESS) {
        if (res & GOPHER_NOT_FOUND) {
            sendErrorResponse(sock, RESOURCE_NOT_FOUND_MSG);
        }
        goto ON_ERROR;
    }
    while ((res = iterateDir(path, &dir, fileName, sizeof(fileName))) == GOPHER_SUCCESS) {
        if (strcmp(fileName, ".") == 0 || strcmp(fileName, "..") == 0) {
            continue;  // Ignora le entry ./ e ../
        }
        filePathSize = strlen(path) + strlen(fileName) + 1;
        if ((filePath = realloc(filePath, filePathSize)) == NULL) {
            goto ON_ERROR;
        }
        if (snprintf(filePath, filePathSize, "%s%s", strcmp(path, "./") == 0 ? "" : path, fileName) < 0) {
            goto ON_ERROR;
        }
        type = gopherType(filePath);
        lineSize = 1 + strlen(fileName) + strlen(filePath) + sizeof(GOPHER_DOMAIN) + sizeof(CRLF) + 7;
        if (lineSize > (line ? strlen(line) : 0)) {
            if ((line = realloc(line, lineSize)) == NULL) {
                sendErrorResponse(sock, SYS_ERR_MSG);
                goto ON_ERROR;
            }
        }
        if (snprintf(line, lineSize, "%c%s\t%s%s\t%s\t%hu" CRLF, type, fileName, filePath, (type == GOPHER_DIR ? "/" : ""), GOPHER_DOMAIN, port) <= 0) {
            sendErrorResponse(sock, SYS_ERR_MSG);
            goto ON_ERROR;
        }
        if (sendAll(sock, line, strlen(line)) == SOCKET_ERROR) {
            goto ON_ERROR;
        }
    }
    if (!(res & GOPHER_END_OF_DIR)) {
        dir = NULL;
        goto ON_ERROR;
    }
    if (send(sock, ".", 1, 0) < 1) {
        goto ON_ERROR;
    }
    free(line);
    free(filePath);
    closeDir(dir);
    closeSocket(sock);
    return GOPHER_SUCCESS;
ON_ERROR:
    if (filePath) {
        free(filePath);
    }
    if (line) {
        free(line);
    }
    if (dir) {
        closeDir(dir);
    }
    return GOPHER_FAILURE;
}

/* Invia il file al client */
void* sendFileTask(void* threadArgs) {
    sendFileArgs args;
    struct sockaddr_in clientAddr;
    size_t clientLen, logSize;
    char *log, address[16];
    args = *(sendFileArgs*)threadArgs;
    free(threadArgs);
    if (
        sendAll(args.dest, args.src, args.size) == SOCKET_ERROR ||
        sendAll(args.dest, CRLF ".", sizeof(CRLF) + 1) == SOCKET_ERROR) {
        closeSocket(args.dest);
        return NULL;
    }
    if (args.src) {
        unmapMem(args.src, args.size);
    }
    clientLen = sizeof(clientAddr);
    if (getpeername(args.dest, (struct sockaddr*)&clientAddr, &clientLen) == SOCKET_ERROR) {
        memset(&clientAddr, 0, clientLen);
    }
    closeSocket(args.dest);
    inetNtoa(&clientAddr.sin_addr, address, sizeof(address));
    logSize = snprintf(NULL, 0, "File: %s | Size: %db | Sent to: %s:%i\n", args.name, args.size, address, clientAddr.sin_port) + 1;
    if ((log = malloc(logSize)) == NULL) {
        return NULL;
    }
    if (snprintf(log, logSize, "File: %s | Size: %db | Sent to: %s:%i\n", args.name, args.size, address, clientAddr.sin_port) > 0) {
        logTransfer(args.pLogger, log);
    }
    free(log);
}

/* Avvia il worker thread di trasmissione */
static int sendFile(const char* name, file_mapping_t* map, int sock, logger_t* pLogger) {
    thread_t tid;
    sendFileArgs* args = NULL;
    if (NULL == (args = malloc(sizeof(sendFileArgs)))) {
        return GOPHER_FAILURE;
    }
    args->src = map->view;
    args->size = map->size;
    args->dest = sock;
    args->pLogger = pLogger;
    strncpy(args->name, name, sizeof(args->name));
    if (_createThread(&tid, (LPTHREAD_START_ROUTINE)sendFileTask, args) != 0) {
        free(args);
        return GOPHER_FAILURE;
    }
    detachThread(tid);
    return GOPHER_SUCCESS;
}

/* Esegue il protocollo. */
int gopher(socket_t sock, int port, logger_t* pLogger) {
    char* selector = NULL;
    file_mapping_t map;
    char buf[BUFF_SIZE];
    size_t bytesRec = 0, selectorSize = 1;
    do {
        if (SOCKET_ERROR == (bytesRec = recv(sock, buf, BUFF_SIZE, 0))) {
            goto ON_ERROR;
        }
        if (NULL == (selector = realloc(selector, selectorSize + bytesRec))) {
            goto ON_ERROR;
        }
        memcpy(selector + selectorSize - 1, buf, bytesRec);
        selectorSize += bytesRec;
        selector[selectorSize - 1] = '\0';
    } while (bytesRec > 0 && !strstr(selector, CRLF));
    printf("Selector: %s_\n", selector);
    if (!validateInput(selector)) {
        sendErrorResponse(sock, BAD_SELECTOR_MSG);
        goto ON_ERROR;
    }
    normalizeInput(selector);
    printf("Request: _%s_\n", selector);
    if (endsWith(selector, "/")) {  // Directory
        if (GOPHER_SUCCESS != sendDir(selector, sock, port)) {
            goto ON_ERROR;
        }
    } else {  // File
        int test;
        if (GOPHER_SUCCESS != (test = isFile(selector))) {
            if (test & GOPHER_NOT_FOUND) {
                sendErrorResponse(sock, RESOURCE_NOT_FOUND_MSG);
            }
            goto ON_ERROR;
        }
        if (
            getFileMap(selector, &map) != GOPHER_SUCCESS ||
            sendFile(selector, &map, sock, pLogger) != GOPHER_SUCCESS) {
            goto ON_ERROR;
        }
    }
    free(selector);
    return GOPHER_SUCCESS;
ON_ERROR:
    closeSocket(sock);
    if (selector) {
        free(selector);
    }
    return GOPHER_FAILURE;
}
