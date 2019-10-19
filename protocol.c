#include "includes/gopher.h"
#include "includes/log.h"

#define MAX_LINE 70

#if defined(_WIN32)

/*****************************************************************************************************************/
/*                                             WINDOWS FUNCTIONS                                                 */

/*****************************************************************************************************************/

void errorRoutine(void* sock) {
    LPSTR err = "3Error retrieving the resource\t\t\r\n.";
    _log(_GENERIC_ERR, ERR, true);
    send(*(SOCKET*)sock, err, strlen(err), 0);
    closeSocket(*(SOCKET*)sock);
    ExitThread(-1);
}

bool isDirectory(WIN32_FIND_DATA* file) {
    /* Windows directory constants */
    return file->dwFileAttributes == 16 || file->dwFileAttributes == 17 || file->dwFileAttributes == 18 || file->dwFileAttributes == 22 || file->dwFileAttributes == 9238;
}

char gopherType(_file* file) {
    LPSTR ext;
    int i;

    if (!strstr(file->name, ".")) {
        return 'X';
    }
    i = strlen(file->name) - 1;
    while (file->name[i] != '.') {
        i--;
    }
    ext = file->name + i + 1;
    if (!strcmp(ext, "txt") || !strcmp(ext, "doc") || !strcmp(ext, "odt") || !strcmp(ext, "rtf") || !strcmp(ext, "c") || !strcmp(ext, "cpp") || !strcmp(ext, "java")) {
        return '0';
    } else if (!strcmp(ext, "hqx")) {
        return '4';
    } else if (!strcmp(ext, "dos")) {
        return '5';
    } else if (!strcmp(ext, "exe") || !strcmp(ext, "jar") || !strcmp(ext, "apk") || !strcmp(ext, "bin") || !strcmp(ext, "bat")) {
        return '9';
    } else if (!strcmp(ext, "gif")) {
        return 'g';
    } else if (!strcmp(ext, "jpg") || !strcmp(ext, "jpeg") || !strcmp(ext, "png")) {
        return 'I';
    } else if (strcmp(ext, "png") == 0) {
        return 'I';
    }
    return 'X';
}

HANDLE readFile(LPCSTR path, SOCKET sock) {
    HANDLE file;
    HANDLE map;
    HANDLE thread;
    DWORD sizeLow;
    DWORD sizeHigh;
    LPVOID view;
    OVERLAPPED ovlp;
    struct sendFileArgs* args;
    if ((file = CreateFile(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE) {
        errorRoutine(&sock);
    }
    if ((sizeLow = GetFileSize(file, &sizeHigh)) == 0) {
        send(sock, ".", 2, 0);
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
    if ((thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)sendFile, args, 0, NULL)) == NULL) {
        errorRoutine(&sock);
    } else {
        return thread;
    }
}

void readDir(LPCSTR path, SOCKET sock) {
    _file file;
    char wildcardPath[MAX_NAME + 2];
    char line[sizeof(file.name) + sizeof(file.path) + sizeof("localhost") + 4];
    LPSTR response;
    size_t responseSize;  // Per il punto finale
    CHAR type;
    WIN32_FIND_DATA data;
    HANDLE hFind;

    response = calloc(1, 1);
    responseSize = 1;  // Per il punto finale
    snprintf(wildcardPath, sizeof(wildcardPath), "%s*", (path[0] == '\0' ? ".\\" : path));
    if ((hFind = FindFirstFile(wildcardPath, &data)) == INVALID_HANDLE_VALUE) {
        errorRoutine(&sock);
    }
    do {
        if (strcmp(data.cFileName, ".") && strcmp(data.cFileName, "..")) {
            strcpy(file.name, data.cFileName);
            strcat(strcpy(file.path, path), file.name);
            type = isDirectory(&data) ? '1' : gopherType(&file);
            snprintf(line, sizeof(line), "%c%s\t%s%s\t%s\r\n", type, file.name, file.path, (type == '1' ? "\\" : ""), "localhost");
            if ((response = realloc(response, responseSize + strlen(line))) == NULL) {
                errorRoutine(&sock);
            }
            responseSize += strlen(line);
            strcat(response, line);
        }
    } while (FindNextFile(hFind, &data));
    strcat(response, ".");
    printf(response);
    send(sock, response, responseSize, 0);
    closesocket(sock);
    CloseHandle(hFind);
    free(response);
}

HANDLE gopher(LPCSTR selector, SOCKET sock) {
    if (strstr(selector, ".\\") || strstr(selector, "..\\") || selector[0] == '\\' || strstr(selector, "\\\\")) {
        errorRoutine(&sock);
    } else if (selector[0] == '\0' || selector[strlen(selector) - 1] == '\\') {  // Directory
        readDir(selector, sock);
    } else {  // File
        return readFile(selector, sock);
    }
    return NULL;
}

#else

/*****************************************************************************************************************/
/*                                             UNIX FUNCTIONS                                                    */

/*****************************************************************************************************************/

void errorRoutine(void* sock) {
    char* err = "3Error retrieving the resource\t\t\r\n.";
    _log(_GENERIC_ERR, ERR, true);
    send(*(int*)sock, err, strlen(err), 0);
    closeSocket(*(int*)sock);
}

char gopherType(const _file* file) {
    char cmdOut[200] = "";
    FILE* response;
    char* command;
    int bytesRead;

    command = malloc(strlen(file->path) + strlen("file \"\"") + 2);
    sprintf(command, "file \"%s\"", file->path);
    response = popen(command, "r");
    bytesRead = fread(cmdOut, 1, 200, response);
    free(command);
    fclose(response);
    if (response == NULL || bytesRead < 0) {
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
    //pthread_detach(tid);
    return tid;
}

void* sendFile(void* sendFileArgs) {
    struct sendFileArgs args;
    void* response;
    struct sockaddr_in clientAddr;
    socklen_t clientLen;
    char log[PIPE_BUF];
    char address[16];
    args = *((struct sendFileArgs*)sendFileArgs);
    free(sendFileArgs);
    if ((response = calloc(args.size + 3, 1)) == NULL) {
        errorRoutine(&args.dest);
    }
    memcpy(response, args.src, args.size);
    strcat((char*)response, "\n.");
    // TODO args.size +2 o +3? Su windows \n diverso che su linux?
    if (send(args.dest, response, args.size + 2, 0) >= 0) {
        clientLen = sizeof(clientAddr);
        getpeername(args.dest, &clientAddr, &clientLen);
        inet_ntop(AF_INET, &clientAddr.sin_addr.s_addr, address, sizeof(clientAddr));
        snprintf(log, PIPE_BUF, "File: %s | Size: %lib | Sent to: %s:%i", args.name, args.size, address, clientAddr.sin_port);
        logTransfer(log);
    }
    munmap(args.src, args.size);
    free(response);
    closeSocket(args.dest);
    printf("invio terminato\n");
}

void readDir(const char* path, int sock) {
    DIR* dir;
    struct dirent* entry;
    _file file;
    char line[sizeof(entry->d_name) + sizeof(file.path) + sizeof("localhost") + 4];
    char* response;
    size_t responseSize;
    char type;

    if ((dir = opendir(path[0] == '\0' ? "./" : path)) == NULL) {
        pthread_exit(NULL);
    }
    response = calloc(1, 1);
    responseSize = 2;  // Per il punto finale
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
            strcat(strcpy(file.path, path), entry->d_name);
            type = gopherType(&file);
            snprintf(line, sizeof(line), "%c%s\t%s%s\t%s\r\n", type, entry->d_name, file.path, (type == '1' ? "/" : ""), "localhost");
            if ((response = realloc(response, responseSize + strlen(line))) == NULL) {
                free(dir);
                pthread_exit(NULL);
            }
            responseSize += strlen(line);
            strcat(response, line);
        }
    }
    strcat(response, ".");
    send(sock, response, responseSize, 0);
    free(dir);
    free(response);
    close(sock);
}

pthread_t gopher(const char* selector, int sock) {
    if (strstr(selector, "./") || strstr(selector, "../") || selector[0] == '/' || strstr(selector, "//")) {
        pthread_exit(NULL);
    } else if (selector[0] == '\0' || selector[strlen(selector) - 1] == '/') {  // Directory
        readDir(selector, sock);
    } else {  // File
        return readFile(selector, sock);
    }
    return -1;
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
