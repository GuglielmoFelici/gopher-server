#include "headers/gopher.h"
#include "headers/log.h"

#define MAX_LINE 70

#if defined(_WIN32)

/*****************************************************************************************************************/
/*                                             WINDOWS FUNCTIONS                                                 */

/*****************************************************************************************************************/

/* Ritorna il carattere di codifica del tipo di file */
char gopherType(WIN32_FIND_DATA* data) {
    LPSTR ext;
    if (data->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        return GOPHER_DIR;
    }
    ext = strrchr(data->cFileName, '.');
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

DWORD sendFile(LPSTR name, struct fileMappingData* map, SOCKET sock) {
    HANDLE thread;
    struct sendFileArgs* args;
    if ((args = malloc(sizeof(struct sendFileArgs))) == NULL) {
        return GOPHER_FAILURE;
    }
    args->src = map->view;
    args->size = map->size;
    args->dest = sock;
    strncpy(args->name, name, sizeof(args->name));
    if ((thread = CreateThread(NULL, 0, sendFileTask, args, 0, NULL)) == NULL) {
        free(args);
        return GOPHER_FAILURE;
    }
    return GOPHER_SUCCESS;
}

/* Mappa il file in memoria */
DWORD getFileMap(LPCSTR path, struct fileMappingData* mapData) {
    HANDLE file = INVALID_HANDLE_VALUE, map = INVALID_HANDLE_VALUE;
    LPVOID view;
    LARGE_INTEGER fileSize;
    OVERLAPPED ovlp;
    if ((file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE) {
        goto ON_ERROR;
    }
    if (!GetFileSizeEx(file, &fileSize)) {
        goto ON_ERROR;
    }
    if (fileSize.QuadPart == 0) {
        mapData->view = NULL;
        mapData->size = 0;
        return GOPHER_SUCCESS;
    }
    memset(&ovlp, 0, sizeof(ovlp));
    if (
        // TODO testare perché possono fallire queste chiamate
        !LockFileEx(file, LOCKFILE_EXCLUSIVE_LOCK, 0, fileSize.LowPart, fileSize.HighPart, &ovlp) ||
        (map = CreateFileMapping(file, NULL, PAGE_READONLY, 0, 0, NULL)) == NULL ||
        !UnlockFileEx(file, 0, fileSize.LowPart, fileSize.HighPart, &ovlp) ||
        !CloseHandle(file) ||
        (view = MapViewOfFile(map, FILE_MAP_READ, 0, 0, 0)) == NULL ||
        !CloseHandle(map)) {
        goto ON_ERROR;
    }
    mapData->view = view;
    mapData->size = fileSize.LowPart;
    return GOPHER_SUCCESS;
ON_ERROR:
    if (file != INVALID_HANDLE_VALUE) {
        CloseHandle(file);
    }
    if (map != INVALID_HANDLE_VALUE) {
        CloseHandle(map);
    }
    return GOPHER_FAILURE;
}

/* Costruisce la lista dei file e la invia al client */
DWORD sendDir(LPCSTR path, SOCKET sock, unsigned short port) {
    char filePath[MAX_NAME];
    WIN32_FIND_DATA data;
    HANDLE hFind = NULL;
    LPSTR line = NULL;
    size_t lineSize;
    CHAR type;
    snprintf(filePath, sizeof(filePath), "%s*", path);
    if (INVALID_HANDLE_VALUE == (hFind = FindFirstFile(filePath, &data))) {
        sendErrorResponse(sock, GetLastError() & ERROR_FILE_NOT_FOUND ? FILE_NOT_FOUND_MSG : SYS_ERR_MSG);
        goto ON_ERROR;
    }
    do {
        if (strcmp(data.cFileName, ".") == 0 || strcmp(data.cFileName, "..") == 0) {
            continue;  // Ignora le entry ./ e ../
        }
        type = gopherType(&data);
        // Compongo la riga di risposta
        lineSize = snprintf(NULL, 0, "%c%s\t%s%s%s\t%s\t%hu" CRLF, type, data.cFileName, path, data.cFileName, DIR_SEP, GOPHER_DOMAIN, port) + 1;
        if ((line = realloc(line, lineSize)) == NULL) {
            sendErrorResponse(sock, SYS_ERR_MSG);
            goto ON_ERROR;
        }
        if (snprintf(line, lineSize, "%c%s\t%s%s%s\t%s\t%hu" CRLF, type, data.cFileName, strcmp(path, ".\\") == 0 ? "" : path, data.cFileName, (type == GOPHER_DIR ? DIR_SEP : ""), GOPHER_DOMAIN, port) < 0) {
            sendErrorResponse(sock, SYS_ERR_MSG);
            goto ON_ERROR;
        }
        if (sendAll(sock, line, strlen(line)) == SOCKET_ERROR) {
            goto ON_ERROR;
        }

    } while (FindNextFile(hFind, &data));
    if (send(sock, ".", 1, 0) < 1) {
        goto ON_ERROR;
    }
    free(line);
    return GOPHER_SUCCESS;
ON_ERROR:
    if (line) {
        free(line);
    }
    if (hFind && hFind != INVALID_HANDLE_VALUE) {
        FindClose(hFind);
    }
    return GOPHER_FAILURE;
}

bool isFile(LPSTR path) {
    DWORD attr;
    return (attr = GetFileAttributes(path)) != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

#else

/*****************************************************************************************************************/
/*                                             LINUX FUNCTIONS                                                    */

/*****************************************************************************************************************/

/* Cleanup e notifica di errore */
void errorRoutine(void* sock) {
    char* err = "3Error (maybe bad request?)\t\t\r\n.";
    send(*(int*)sock, err, strlen(err), 0);
    closeSocket(*(int*)sock);
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
    if (strstr(cmdOut, FILE_CMD_EXEC1) || strstr(cmdOut, FILE_CMD_EXEC2)) {
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

int getFileMap(char* path, struct fileMappingData* mapData) {
    void* map;
    int fd;
    struct stat statBuf;
    struct sendFileArgs* args;
    pthread_t tid;
    if (
        // TODO VERIFICARE QUESTA CASCATA
        (fd = open(path, O_RDONLY)) < 0 ||
        flock(fd, LOCK_EX) < 0 ||
        fstat(fd, &statBuf) < 0 ||
        (map = mmap(NULL, statBuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED ||
        flock(fd, LOCK_UN) < 0 ||
        close(fd) < 0) {
        return GOPHER_FAILURE;
    }
    mapData->view = map;
    mapData->size = statBuf.st_size;
    return GOPHER_SUCCESS;
}

/* Invia il file al client */
void* sendFileTask(void* threadArgs) {
    struct sendFileArgs args;
    void* response;
    int sent = 0;
    struct sockaddr_in clientAddr;
    socklen_t clientLen;
    args = *(struct sendFileArgs*)threadArgs;
    free(threadArgs);
    if (
        sendAll(args.dest, (char*)args.src, args.size) == SOCKET_ERROR ||
        sendAll(args.dest, CRLF ".", sizeof(CRLF) + 1) == SOCKET_ERROR ||
        close(args.dest) == SOCKET_ERROR) {
        pthread_exit(GOPHER_FAILURE);
    }
    if (args.src) {
        munmap(args.src, args.size);
    }
    if (send(args.dest, response, args.size + 2, O_NONBLOCK) >= 0) {
        char log[PIPE_BUF];
        char address[16];
        clientLen = sizeof(clientAddr);
        getpeername(args.dest, &clientAddr, &clientLen);
        inet_ntop(AF_INET, &clientAddr.sin_addr.s_addr, address, sizeof(clientAddr));
        snprintf(log, PIPE_BUF, "File: %s | Size: %lib | Sent to: %s:%i\n", args.name, args.size, address, clientAddr.sin_port);
        logTransfer(log);
    }
    closeSocket(args.dest);
}

/* Avvia il worker thread di trasmissione */
int sendFile(const char* path, struct fileMappingData* map, int sock) {
    pthread_t tid;
    struct sendFileArgs* args;
    if ((args = malloc(sizeof(struct sendFileArgs))) == NULL) {
        return GOPHER_FAILURE;
    }
    args->src = map->view;
    args->size = map->size;
    args->dest = sock;
    strncpy(args->name, path, MAX_NAME);
    if (pthread_create(&tid, NULL, sendFileTask, args) != 0) {
        return GOPHER_FAILURE;
    }
    return GOPHER_SUCCESS;
}

/* Costruisce la lista dei file e la invia al client */
int sendDir(const char* path, int sock, unsigned short port) {
    DIR* dir = NULL;
    struct dirent* entry;
    char filePath[MAX_NAME];
    char* line = NULL;
    size_t lineSize;
    char type;
    if ((dir = opendir(path)) == NULL) {
        return GOPHER_FAILURE;
    }
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;  // Ignora le entry ./ e ../
        }
        lineSize = snprintf(NULL, 0, "%c%s\t%s%s%s\t%s\t%hu" CRLF, type, entry->d_name, path, entry->d_name, DIR_SEP, GOPHER_DOMAIN, port) + 1;
        if ((line = realloc(line, lineSize)) == NULL) {
            sendErrorResponse(sock, SYS_ERR_MSG);
            goto ON_ERROR;
        }
        if (snprintf(line, lineSize, "%c%s\t%s%s%s\t%s\t%hu" CRLF, type, entry->d_name, strcmp(path, "./") == 0 ? "" : path, entry->d_name, (type == GOPHER_DIR ? DIR_SEP : ""), GOPHER_DOMAIN, port) < 0) {
            sendErrorResponse(sock, SYS_ERR_MSG);
            goto ON_ERROR;
        }
        if (sendAll(sock, line, strlen(line)) == SOCKET_ERROR) {
            goto ON_ERROR;
        }
    }
    if (send(sock, ".", 1, 0) < 1) {
        goto ON_ERROR;
    }
    free(line);
    closedir(dir);
    close(sock);
    return GOPHER_SUCCESS;
ON_ERROR:
    if (line) {
        free(line);
    }
    if (dir) {
        closedir(dir);
    }
    return GOPHER_FAILURE;
}

bool isFile(char* path) {
    return true;  //TODO
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
        closeSocket(sock);
    } else {  // File
        if (!isFile(selector)) {
            sendErrorResponse(sock, FILE_NOT_FOUND_MSG);
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
    if (selector) {
        free(selector);
    }
    return GOPHER_FAILURE;
}
