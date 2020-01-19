#include "headers/gopher.h"
#include "headers/log.h"

#define MAX_LINE 70

#if defined(_WIN32)

/*****************************************************************************************************************/
/*                                             WINDOWS FUNCTIONS                                                 */

/*****************************************************************************************************************/

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

/* Invia il file al client */
DWORD WINAPI sendFileTask(LPVOID sendFileArgs) {
    struct sendFileArgs args;
    char log[PIPE_BUF];  // TODO -.-
    char address[IP_ADDRESS_LENGTH];
    struct sockaddr_in clientAddr;
    int clientLen;
    args = *(struct sendFileArgs*)sendFileArgs;
    free(sendFileArgs);
    if (
        sendAll(args.dest, (char*)args.src, args.size) == SOCKET_ERROR ||
        sendAll(args.dest, CRLF ".", sizeof(CRLF) + 1) == SOCKET_ERROR ||
        closesocket(args.dest) == SOCKET_ERROR) {
        ExitThread(GOPHER_FAILURE);
    }
    if (args.src) {
        UnmapViewOfFile(args.src);
    }
    clientLen = sizeof(clientAddr);
    if (getpeername(args.dest, (struct sockaddr*)&clientAddr, &clientLen) == SOCKET_ERROR) {
        ExitThread(GOPHER_FAILURE);
    }
    strncpy(address, inet_ntoa(clientAddr.sin_addr), sizeof(address));
    snprintf(log, PIPE_BUF, "File: %s | Size: %lib | Sent to: %s:%i\n", args.name, args.size, address, clientAddr.sin_port);
    logTransfer(log);
    // TODO closeSocket(args.dest); ????
}

#else

/*****************************************************************************************************************/
/*                                             LINUX FUNCTIONS                                                    */

/*****************************************************************************************************************/

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

/* Invia il file al client */
void* sendFileTask(void* threadArgs) {
    struct sendFileArgs args;
    struct sockaddr_in clientAddr;
    socklen_t clientLen;
    size_t logSize;
    char *log, address[16];
    int ret = GOPHER_FAILURE;
    args = *(struct sendFileArgs*)threadArgs;
    free(threadArgs);
    if (
        sendAll(args.dest, args.src, args.size) == SOCKET_ERROR ||
        sendAll(args.dest, CRLF ".", sizeof(CRLF) + 1) == SOCKET_ERROR) {
        close(args.dest);
        pthread_exit(&ret);
    }
    close(args.dest);
    if (args.src) {
        munmap(args.src, args.size);
    }
    clientLen = sizeof(clientAddr);
    getpeername(args.dest, &clientAddr, &clientLen);
    inet_ntop(AF_INET, &clientAddr.sin_addr.s_addr, address, sizeof(clientAddr));
    logSize = snprintf(NULL, 0, "File: %s | Size: %db | Sent to: %s:%i\n", args.name, args.size, address, clientAddr.sin_port) + 1;
    if ((log = malloc(logSize)) == NULL) {
        pthread_exit(&ret);
    }
    if (snprintf(log, logSize, "File: %s | Size: %db | Sent to: %s:%i\n", args.name, args.size, address, clientAddr.sin_port) > 0) {
        logTransfer(log);
    }
    free(log);
    closeSocket(args.dest);
}

#endif

/*****************************************************************************************************************/
/*                                             COMMON                                                            */

/*****************************************************************************************************************/

_string trimEnding(_string str) {
    for (int i = strlen(str) - 1; i >= 0; i--) {
        if (str[i] == ' ' || str[i] == '\r' || str[i] == '\n') {
            str[i] = '\0';
        }
    }
    return str;
}

bool validateInput(_string str) {
    _string strtokptr;
    _string ret;
    ret = strtok_r(str, CRLF, &strtokptr);
    return ret == NULL ||
           !strstr(ret, ".\\") && !strstr(ret, "./");
}

void normalizeInput(_string str) {
    _string strtokptr;
    if (strncmp(str, CRLF, sizeof(CRLF)) == 0) {
        strcpy(str, "." DIR_SEP);
    } else {
        strtok_r(str, CRLF, &strtokptr);
    }
}

int sendErrorResponse(_socket sock, _string msg) {
    if (sendAll(sock, ERROR_MSG " - ", sizeof(ERROR_MSG) + 3) == SOCKET_ERROR) {
        return SOCKET_ERROR;
    }
    if (sendAll(sock, msg, strlen(msg)) == SOCKET_ERROR) {
        return SOCKET_ERROR;
    }
    if (sendAll(sock, CRLF ".", 3) == SOCKET_ERROR) {
        return SOCKET_ERROR;
    }
    if (closeSocket(sock) == SOCKET_ERROR) {
        return SOCKET_ERROR;
    }
}

/* Costruisce la lista dei file e la invia al client */
int sendDir(const char* path, int sock, unsigned short port) {
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
        if (snprintf(filePath, filePathSize, "%s%s", strcmp(path, "." DIR_SEP) == 0 ? "" : path, fileName) < 0) {
            goto ON_ERROR;
        }
        type = gopherType(filePath);
        lineSize = 1 + strlen(fileName) + strlen(filePath) + sizeof(DIR_SEP) + sizeof(GOPHER_DOMAIN) + sizeof(CRLF) + 6;
        if (lineSize > (line ? strlen(line) : 0)) {
            if ((line = realloc(line, lineSize)) == NULL) {
                sendErrorResponse(sock, SYS_ERR_MSG);
                goto ON_ERROR;
            }
        }
        if (snprintf(line, lineSize, "%c%s\t%s%s\t%s\t%hu" CRLF, type, fileName, filePath, (type == GOPHER_DIR ? DIR_SEP : ""), GOPHER_DOMAIN, port) <= 0) {
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

/* Avvia il worker thread di trasmissione */
int sendFile(const char* path, struct fileMappingData* map, int sock) {
    _thread tid;
    struct sendFileArgs* args;
    if ((args = malloc(sizeof(struct sendFileArgs))) == NULL) {
        return GOPHER_FAILURE;
    }
    args->src = map->view;
    args->size = map->size;
    args->dest = sock;
    strncpy(args->name, path, sizeof(args->name));
    if (_createThread(&tid, sendFileTask, args) != 0) {
        free(args);
        return GOPHER_FAILURE;
    }
    detachThread(tid);
    return GOPHER_SUCCESS;
}

/* Esegue il protocollo. */
int gopher(_socket sock, unsigned short port) {
    _string selector = NULL;
    struct fileMappingData map;
    char buf[BUFF_SIZE];
    size_t bytesRec = 0, selectorSize = 1;
    do {
        if (
            (bytesRec = recv(sock, buf, BUFF_SIZE, 0)) == SOCKET_ERROR ||
            (selector = realloc(selector, selectorSize + bytesRec)) == NULL) {
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
    printf("Request: _%s_\n", strlen(selector) == 0 ? "_empty" : selector);
    if (endsWith(selector, DIR_SEP)) {  // Directory
        if (sendDir(selector, sock, port) != GOPHER_SUCCESS) {
            goto ON_ERROR;
        }
    } else {  // File
        if (isFile(selector) != GOPHER_SUCCESS) {
            sendErrorResponse(sock, RESOURCE_NOT_FOUND_MSG);
            goto ON_ERROR;
        }
        if (
            getFileMap(selector, &map) != GOPHER_SUCCESS ||
            sendFile(selector, &map, sock) != GOPHER_SUCCESS) {
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
