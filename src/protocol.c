#include "../headers/protocol.h"
#include <stdbool.h>
#include <stdio.h>
#include "../headers/datatypes.h"
#include "../headers/logger.h"
#include "../headers/platform.h"

#define MAX_LINE 70

#if defined(_WIN32)

/*****************************************************************************************************************/
/*                                             WINDOWS FUNCTIONS                                                 */

/*****************************************************************************************************************/

#include <windows.h>

/* Ritorna il carattere di codifica del tipo di file */
static char gopherType(LPCSTR filePath) {
    LPSTR ext;
    if (fileAttributes(filePath) & PLATFORM_ISDIR) {
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

#include <string.h>

/* Ritorna il carattere di codifica del tipo di file */
static char gopherType(const char* file) {
    if (fileAttributes(file) & PLATFORM_ISDIR) {
        return GOPHER_DIR;
    }
    char cmd[MAX_NAME];
    FILE* response;
    int bytesRead;
    if (snprintf(cmd, sizeof(cmd), "file \"%s\"", file) < 0) {
        return GOPHER_UNKNOWN;
    }
    response = popen(cmd, "r");
    bytesRead = fread(cmd, 1, sizeof(cmd), response);
    fclose(response);
    if (strstr(cmd, FILE_CMD_NOT_FOUND)) {
        return GOPHER_UNKNOWN;
    } else if (strstr(cmd, FILE_CMD_EXEC1) || strstr(cmd, FILE_CMD_EXEC2)) {
        return GOPHER_BINARY;
    } else if (strstr(cmd, FILE_CMD_IMG)) {
        return GOPHER_IMAGE;
    } else if (strstr(cmd, FILE_CMD_GIF)) {
        return GOPHER_GIF;
    } else if (strstr(cmd, FILE_CMD_TXT)) {
        return GOPHER_TEXT;
    }
    return GOPHER_UNKNOWN;
}

#endif

/*****************************************************************************************************************/
/*                                             COMMON                                                            */

/*****************************************************************************************************************/

/* Rimuove CRLF; se la stringa è vuota, ci scrive "."; 
   cambia tutti i backslash in forward slash; rimuove i trailing slash */
static int normalizePath(string_t str) {
    if (!str) {
        return GOPHER_FAILURE;
    }
    if (strncmp(str, CRLF, sizeof(CRLF)) == 0) {
        strcpy(str, ".");
    } else {
        string_t strtokptr;
        strtok_r(str, CRLF, &strtokptr);
        int len = strlen(str);
        for (int i = 0; i < len; i++) {
            if (str[i] == '\\') {
                str[i] = '/';
            }
        }
        if (str[len - 1] == '/') {
            str[len - 1] = '\0';
        }
    }
    return GOPHER_SUCCESS;
}

static bool validateInput(string_t str) {
    string_t strtokptr, ret;
    ret = strtok_r(str, CRLF, &strtokptr);
    return ret == NULL ||
           !strstr(ret, "..") && !strstr(ret, ".\\") && !strstr(ret, "./") && ret[strlen(ret) - 1] != '.';
}

static int sendErrorResponse(socket_t sock, cstring_t msg) {
    if (SOCKET_ERROR == sendAll(sock, ERROR_MSG " - ", sizeof(ERROR_MSG) + 3)) {
        return GOPHER_FAILURE;
    }
    if (SOCKET_ERROR == sendAll(sock, msg, strlen(msg))) {
        return GOPHER_FAILURE;
    }
    if (SOCKET_ERROR == sendAll(sock, CRLF ".", 3)) {
        return GOPHER_FAILURE;
    }
    if (PLATFORM_FAILURE == closeSocket(sock)) {
        return GOPHER_FAILURE;
    }
    return GOPHER_SUCCESS;
}

/* Costruisce la lista dei file e la invia al client */
static int sendDir(cstring_t path, int sock, int port) {
    if (!path) {
        return GOPHER_FAILURE;
    }
    _dir dir = NULL;
    char filePath[MAX_NAME];
    char *fileName = NULL, *line = NULL;
    size_t lineSize, filePathSize;
    int res;
    while (PLATFORM_SUCCESS == (res = iterateDir(path, &dir, filePath, sizeof(filePath)))) {
        normalizePath(filePath);
        if (NULL == (fileName = strrchr(filePath, '/'))) {  // Isola il nome del file (dopo l'ultimo "/"")
            fileName = filePath;
        } else {
            fileName++;
        }
        if (strcmp(fileName, ".") == 0 || strcmp(fileName, "..") == 0) {
            continue;  // Ignora le entry ./ e ../
        }
        char type = gopherType(filePath);
        lineSize = 1 + strlen(fileName) + strlen(filePath) + sizeof(GOPHER_DOMAIN) + sizeof(CRLF) + 6;
        if (lineSize > (line ? strlen(line) : 0)) {
            if (NULL == (line = realloc(line, lineSize))) {
                sendErrorResponse(sock, SYS_ERR_MSG);
                goto ON_ERROR;
            }
        }
        if (snprintf(line, lineSize, "%c%s\t%s\t%s\t%hu" CRLF, type, fileName, filePath, GOPHER_DOMAIN, port) <= 0) {
            sendErrorResponse(sock, SYS_ERR_MSG);
            goto ON_ERROR;
        }
        if (SOCKET_ERROR == sendAll(sock, line, strlen(line))) {
            goto ON_ERROR;
        }
    }
    if (!(res & PLATFORM_END_OF_DIR)) {
        dir = NULL;
        sendErrorResponse(sock, SYS_ERR_MSG);
        goto ON_ERROR;
    }
    if (send(sock, ".", 1, 0) < 1) {
        goto ON_ERROR;
    }
    free(line);
    closeDir(dir);
    closeSocket(sock);
    return GOPHER_SUCCESS;
ON_ERROR:
    if (line) {
        free(line);
    }
    if (dir) {
        closeDir(dir);
    }
    return GOPHER_FAILURE;
}

/* Invia il file al client */
// TODO static? per windows si
static void* sendFileTask(void* threadArgs) {
    send_args_t args;
    struct sockaddr_in clientAddr;
    int clientLen;
    size_t logSize;
    string_t log;
    char address[16];
    args = *(send_args_t*)threadArgs;
    free(threadArgs);
    if (
        sendAll(args.dest, args.src, args.size) == SOCKET_ERROR ||
        sendAll(args.dest, CRLF ".", 3) == SOCKET_ERROR) {
        closeSocket(args.dest);
        return NULL;
    }
    if (args.src) {
        unmapMem(args.src, args.size);
    }
    clientLen = sizeof(clientAddr);
    if (SOCKET_ERROR == getpeername(args.dest, (struct sockaddr*)&clientAddr, &clientLen)) {
        memset(&clientAddr, 0, clientLen);
    }
    closeSocket(args.dest);
    inetNtoa(&clientAddr.sin_addr, address, sizeof(address));
    string_t logFormat = "File: %s | Size: %db | Sent to: %s:%i\n";
    logSize = snprintf(NULL, 0, logFormat, args.name, args.size, address, clientAddr.sin_port) + 1;
    if ((log = malloc(logSize)) == NULL) {
        return NULL;
    }
    if (snprintf(log, logSize, logFormat, args.name, args.size, address, clientAddr.sin_port) > 0) {
        logTransfer(args.pLogger, log);
    }
    free(log);
}

/* Avvia il worker thread di trasmissione */
static int sendFile(cstring_t name, const file_mapping_t* map, int sock, const logger_t* pLogger) {
    thread_t tid;
    send_args_t* args = NULL;
    if (!map) {
        return GOPHER_FAILURE;
    }
    if (NULL == (args = malloc(sizeof(send_args_t)))) {
        return GOPHER_FAILURE;
    }
    args->src = map->view;
    args->size = map->size;
    args->dest = sock;
    args->pLogger = pLogger;
    strncpy(args->name, name, sizeof(args->name));
    if (PLATFORM_SUCCESS != startThread(&tid, (LPTHREAD_START_ROUTINE)sendFileTask, args)) {
        free(args);
        return GOPHER_FAILURE;
    }
    detachThread(tid);
    return GOPHER_SUCCESS;
}

/* Esegue il protocollo. */
int gopher(socket_t sock, int port, const logger_t* pLogger) {
    string_t selector = NULL;
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
    if (!validateInput(selector)) {
        sendErrorResponse(sock, INVALID_SELECTOR);
        goto ON_ERROR;
    }
    normalizePath(selector);
    int fileAttr = fileAttributes(selector);
    if (PLATFORM_FILE_ERR & fileAttr) {
        sendErrorResponse(sock, PLATFORM_NOT_FOUND & fileAttr ? RESOURCE_NOT_FOUND_MSG : SYS_ERR_MSG);
        goto ON_ERROR;
    } else if (PLATFORM_ISDIR & fileAttr) {  // Directory
        if (GOPHER_SUCCESS != sendDir(selector, sock, port)) {
            goto ON_ERROR;
        }
    } else {  // File
        file_mapping_t map;
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
