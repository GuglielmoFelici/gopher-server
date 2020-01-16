#include "headers/gopher.h"
#include "headers/log.h"

#define MAX_LINE 70

#if defined(_WIN32)

/*****************************************************************************************************************/
/*                                             WINDOWS FUNCTIONS                                                 */

/*****************************************************************************************************************/

void errorRoutine(void* sock) {
    LPSTR err = "3Error retrieving the resource\t\t\r\n.";
    send(*(SOCKET*)sock, err, strlen(err), 0);
    closeSocket(*(SOCKET*)sock);
    ExitThread(-1);
}

DWORD sendErrorResponse(SOCKET sock, LPSTR msg) {
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

/* Ritorna il carattere di codifica del tipo di file */
char gopherType(WIN32_FIND_DATA* fileData) {
    LPSTR ext;
    if (fileData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        return GOPHER_DIR;
    }
    ext = strrchr(fileData->cFileName, '.');
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

bool validateInput(LPSTR str) {
    LPSTR strtokptr;
    LPSTR ret;
    ret = strtok_r(str, CRLF, &strtokptr);
    return ret == NULL ||
           !strstr(ret, ".\\") && !strstr(ret, "./");
}

void normalizeInput(LPSTR str) {
    LPSTR strtokptr;
    if (strncmp(str, CRLF, sizeof(CRLF)) == 0) {
        strcpy(str, "." DIR_SEP);
    } else {
        strtok_r(str, CRLF, &strtokptr);
    }
}

bool isFile(LPSTR path) {
    DWORD attr;
    return (attr = GetFileAttributes(path)) != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

/* Esegue il protocollo. */
int gopher(SOCKET sock, unsigned short port) {
    LPSTR selector = NULL;
    struct fileMappingData map;
    char buf[BUFF_SIZE];
    size_t bytesRec = 0, selectorSize = 1;
    do {
        if (
            (bytesRec = recv(sock, buf, BUFF_SIZE, 0)) == SOCKET_ERROR ||
            (selector = realloc(selector, selectorSize + bytesRec)) == NULL) {
            sendErrorResponse(sock, SYS_ERR_MSG);
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
        closesocket(sock);
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
char gopherType(const _file* file) {
    char cmdOut[200] = "";
    FILE* response;
    char* command;
    int bytesRead;
    size_t cmdSize = strlen(file->path) + strlen("file \"\"") + 1;
    command = malloc(cmdSize);
    snprintf(command, cmdSize, "file \"%s\"", file->path);
    response = popen(command, "r");
    bytesRead = fread(cmdOut, 1, 200, response);
    free(command);
    fclose(response);
    if (bytesRead < 0) {
        pthread_exit(NULL);
    }
    if (strstr(cmdOut, "executable") || strstr(cmdOut, "ELF")) {
        return '9';
    } else if (strstr(cmdOut, "image")) {
        return 'I';
    } else if (strstr(cmdOut, "directory")) {
        return '1';
    } else if (strstr(cmdOut, "GIF")) {
        return 'G';
    } else if (strstr(cmdOut, "text")) {
        return '0';
    }
    return 'X';
}

/* Invia il file al client */
void* sendFile(void* sendFileArgs) {
    struct sendFileArgs args;
    void* response;
    int sent = 0;
    struct sockaddr_in clientAddr;
    socklen_t clientLen;
    args = *((struct sendFileArgs*)sendFileArgs);
    free(sendFileArgs);
    if ((response = calloc(args.size + 3, 1)) == NULL) {
        pthread_exit(NULL);
    }
    memcpy(response, args.src, args.size);
    strcat((char*)response, "\n.");
    if (send(args.dest, response, args.size + 2, O_NONBLOCK) >= 0) {
        char log[PIPE_BUF];
        char address[16];
        clientLen = sizeof(clientAddr);
        getpeername(args.dest, &clientAddr, &clientLen);
        inet_ntop(AF_INET, &clientAddr.sin_addr.s_addr, address, sizeof(clientAddr));
        snprintf(log, PIPE_BUF, "File: %s | Size: %lib | Sent to: %s:%i\n", args.name, args.size, address, clientAddr.sin_port);
        logTransfer(log);
    }
    munmap(args.src, args.size);
    free(response);
    closeSocket(args.dest);
}

/* Mappa il file in memoria e avvia il worker thread di trasmissione */
pthread_t readFile(const char* path, int sock) {
    void* map;
    int fd;
    struct stat statBuf;
    struct sendFileArgs* args;
    pthread_t tid;
    if ((fd = open(path, O_RDONLY)) < 0) {
        pthread_exit(NULL);
    }
    if (flock(fd, LOCK_EX) < 0) {
        pthread_exit(NULL);
    }
    if (fstat(fd, &statBuf) < 0) {
        pthread_exit(NULL);
    }
    if (statBuf.st_size == 0) {
        send(sock, ".", 2, 0);
        if (close(fd) < 0) {
            pthread_exit(NULL);
        }
        close(sock);
        return -1;
    }
    if ((map = mmap(NULL, statBuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
        pthread_exit(NULL);
    }
    if (flock(fd, LOCK_UN) < 0) {
        pthread_exit(NULL);
    }
    if (close(fd) < 0) {
        pthread_exit(NULL);
    }
    args = malloc(sizeof(struct sendFileArgs));
    args->src = map;
    args->size = statBuf.st_size;
    args->dest = sock;
    strncpy(args->name, path, MAX_NAME);
    if (pthread_create(&tid, NULL, sendFile, args)) {
        pthread_exit(NULL);
    }
    return tid;
}

/* Costruisce la lista dei file e la invia al client */
void readDir(const char* path, int sock) {
    DIR* dir;
    struct dirent* entry;
    _file file;
    char line[sizeof(entry->d_name) + sizeof(file.path) + sizeof(GOPHER_DOMAIN) + 4];
    char* response;
    char type;
    size_t responseSize;
    if ((dir = opendir(path[0] == '\0' ? "./" : path)) == NULL) {
        pthread_exit(NULL);
    }
    response = calloc(1, 1);
    responseSize = 2;  // Per il punto finale
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
            strcat(strcpy(file.path, path), entry->d_name);
            type = gopherType(&file);
            snprintf(line, sizeof(line), "%c%s\t%s%s\t%s" CRLF, type, entry->d_name, file.path, (type == '1' ? "/" : ""), GOPHER_DOMAIN);
            if ((response = realloc(response, responseSize + strlen(line))) == NULL) {
                closedir(dir);
                pthread_exit(NULL);
            }
            responseSize += strlen(line);
            strcat(response, line);
        }
    }
    strcat(response, ".");
    send(sock, response, responseSize, 0);
    closedir(dir);
    free(response);
    close(sock);
}

/* Valida il selettore ed esegue il protocollo. Ritorna l'identificativo dell'ultimo thread generato */
pthread_t gopher(int sock) {
    char selector[MAX_GOPHER_MSG];
    recv(sock, selector, MAX_GOPHER_MSG, 0);
    trimEnding(selector);
    printf("Request: %s\n", selector);
    if (strstr(selector, "./") || strstr(selector, "../") || selector[0] == '/' || strstr(selector, "//")) {
        pthread_exit(NULL);
    } else if (selector[0] == '\0' || selector[strlen(selector) - 1] == '/') {  // Directory
        readDir(selector, sock);
        return -1;
    } else {  // File
        return readFile(selector, sock);
    }
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
