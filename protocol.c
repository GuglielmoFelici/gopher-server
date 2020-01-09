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

bool isDirectory(LPCSTR path) {
    return GetFileAttributes(path) & FILE_ATTRIBUTE_DIRECTORY;
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
char gopherType(LPSTR name) {
    LPSTR ext;
    if (!strstr(name, ".")) {
        return GOPHER_UNKNOWN;
    }
    ext = strrchr(name, '.') + 1;  // Isola l'estensione del file
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
void* sendFile(void* sendFileArgs) {
    struct sendFileArgs args;
    char* response;
    args = *((struct sendFileArgs*)sendFileArgs);
    free(sendFileArgs);
    if ((response = calloc(args.size + 3, 1)) == NULL) {
        errorRoutine(&args.dest);
    }
    memcpy((void*)response, args.src, args.size);
    strcat(response, "\n.");
    if (send(args.dest, response, strlen(response), 0) >= 0) {
        // Logga il trasferimento
        char log[PIPE_BUF];
        char address[16];
        struct sockaddr_in clientAddr;
        int clientLen;
        clientLen = sizeof(clientAddr);
        getpeername(args.dest, (struct sockaddr*)&clientAddr, &clientLen);
        strncpy(address, inet_ntoa(clientAddr.sin_addr), sizeof(address));
        snprintf(log, PIPE_BUF, "File: %s | Size: %lib | Sent to: %s:%i\n", args.name, args.size, address, clientAddr.sin_port);
        logTransfer(log);
    }
    UnmapViewOfFile(args.src);
    free(response);
    closeSocket(args.dest);
}

/* Mappa il file in memoria e avvia il worker thread di trasmissione */
HANDLE readFile(LPCSTR path, SOCKET sock, bool asyncSend) {
    HANDLE file;
    HANDLE map;
    HANDLE thread;
    DWORD sizeLow;
    DWORD sizeHigh;
    LPVOID view;
    OVERLAPPED ovlp;
    struct sendFileArgs* args;
    if ((file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE) {
        errorRoutine(&sock);
    }
    if ((sizeLow = GetFileSize(file, &sizeHigh)) == 0) {
        send(sock, ".", 2, 0);
        // Logga
        char log[PIPE_BUF];
        char address[16];
        struct sockaddr_in clientAddr;
        int clientLen;
        clientLen = sizeof(clientAddr);
        getpeername(sock, (struct sockaddr*)&clientAddr, &clientLen);
        strncpy(address, inet_ntoa(clientAddr.sin_addr), sizeof(address));
        snprintf(log, PIPE_BUF, "File: %s | Size: %lib | Sent to: %s:%i\n", path, 0, address, clientAddr.sin_port);
        logTransfer(log);
        if (!CloseHandle(file)) {
            errorRoutine(&sock);
        }
        closeSocket(sock);
        return NULL;
    }
    memset(&ovlp, 0, sizeof(ovlp));
    if (!LockFileEx(file, LOCKFILE_EXCLUSIVE_LOCK, 0, sizeLow, sizeHigh, &ovlp)) {
        CloseHandle(file);
        errorRoutine(&sock);
    }
    if ((map = CreateFileMapping(file, NULL, PAGE_READONLY, 0, 0, NULL)) == NULL) {
        CloseHandle(file);
        errorRoutine(&sock);
    }
    if (!UnlockFileEx(file, 0, sizeLow, sizeHigh, &ovlp)) {
        CloseHandle(file);
        errorRoutine(&sock);
    }
    if (!CloseHandle(file)) {
        errorRoutine(&sock);
    }
    if ((view = MapViewOfFile(map, FILE_MAP_READ, 0, 0, 0)) == NULL) {
        errorRoutine(&sock);
    }
    if (!CloseHandle(map)) {
        errorRoutine(&sock);
    }
    if ((args = malloc(sizeof(struct sendFileArgs))) == NULL) {
        errorRoutine(&sock);
    }
    args->src = view;
    args->dest = sock;
    args->size = sizeLow;
    strncpy(args->name, path, sizeof(args->name));
    if ((thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)sendFile, args, 0, NULL)) == NULL) {
        errorRoutine(&sock);
    } else if (asyncSend) {
        WaitForSingleObject(thread, INFINITE);
    }
}

/* Costruisce la lista dei file e la invia al client */
DWORD sendDir(LPCSTR path, SOCKET sock) {
    char filePath[MAX_NAME];
    LPSTR line;
    size_t lineSize;
    CHAR type;
    WIN32_FIND_DATA data;
    HANDLE hFind;
    snprintf(filePath, sizeof(filePath), "%s*", (path[0] == '\0' ? ".\\" : path));
    if (INVALID_HANDLE_VALUE == (hFind = FindFirstFile(filePath, &data))) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            sendErrorResponse(sock, FILE_NOT_FOUND_MSG);
        }
        return GOPHER_FAILURE;
    }
    if ((line = malloc(1)) == NULL) {
        return GOPHER_FAILURE;
    }
    do {
        if (strcmp(data.cFileName, ".") && strcmp(data.cFileName, "..")) {  // Ignora le entry ./ e ../
            snprintf(filePath, sizeof(filePath), "%s\\%s", (path[0] == '\0' ? "." : path), data.cFileName);
            type = isDirectory(filePath) ? '1' : gopherType(data.cFileName);
            // Compongo la riga di risposta
            lineSize = strlen(data.cFileName) + strlen(path) + strlen(data.cFileName) + strlen(GOPHER_DOMAIN) + 7;
            if (line = realloc(line, lineSize)) {
                snprintf(line, lineSize, "%c%s\t%s%s%s\t%s\r\n", type, data.cFileName, path, data.cFileName, (type == '1' ? "\\" : ""), GOPHER_DOMAIN);
            } else {
                return GOPHER_FAILURE;
            }
            if (sendAll(sock, line, strlen(line)) == SOCKET_ERROR) {
                free(line);
                return GOPHER_FAILURE;
            }
        }
    } while (FindNextFile(hFind, &data));
    free(line);
    FindClose(hFind);
    if (send(sock, ".", 1, 0) < 1) {
        return GOPHER_FAILURE;
    }
    closesocket(sock);
    return GOPHER_SUCCESS;
}

bool validateInput(LPSTR str) {
    LPSTR saveptr;
    LPSTR ret;
    if (str == NULL) {
        return false;
    } else if (strcmp(str, "\r\n") == 0) {
        str[0] = '\0';
        return true;
    } else {
        ret = strtok_r(str, "\r\n", &saveptr);
        return (!strstr(ret, "..\\") && ret[0] != '\\');
    }
}

/* Valida la stringa ed esegue il protocollo. */
int gopher(SOCKET sock, bool asyncSend) {
    LPSTR selector, response;
    char buf[BUFF_SIZE];
    DWORD exitCode = 0, responseSize = 0;
    size_t bytesRec = 0, selectorSize = 1;
    HANDLE sendThread = NULL;
    selector = malloc(1);
    if (selector == NULL) {
        goto ON_ERROR;
    }
    do {
        bytesRec = recv(sock, buf, BUFF_SIZE, 0);
        if (bytesRec < 0) {
            goto ON_ERROR;
        }
        if ((selector = realloc(selector, selectorSize + bytesRec)) == NULL) {
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
    printf("Request: _%s_\n", strlen(selector) == 0 ? "_empty" : selector);
    if (selector[0] == '\0' || selector[strlen(selector) - 1] == '\\') {  // Directory
        if (sendDir(selector, sock) != GOPHER_SUCCESS) {
            goto ON_ERROR;
        }
    } else {  // File
        if (readFile(selector, sock, asyncSend) != GOPHER_SUCCESS) {
            goto ON_ERROR;
        }
    }
    free(selector);
    if (!asyncSend) {
        closesocket(sock);
    }
    return GOPHER_SUCCESS;
ON_ERROR:
    if (selector) {
        free(selector);
    }
    closesocket(sock);
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
            snprintf(line, sizeof(line), "%c%s\t%s%s\t%s\r\n", type, entry->d_name, file.path, (type == '1' ? "/" : ""), GOPHER_DOMAIN);
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
