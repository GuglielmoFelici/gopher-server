#include "includes/gopher.h"

#define MAX_LINE 70

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

void errorRoutine(void* sock) {
    char* err = "3Error retrieving the resource\t\t\r\n.";
    _log(_THREAD_ERR, ERR, true);
    send(*(_socket*)sock, err, strlen(err), 0);
    closeSocket(*(_socket*)sock);
    return;
}

#if defined(_WIN32)

/*****************************************************************************************************************/
/*                                             WINDOWS FUNCTIONS                                                 */

/*****************************************************************************************************************/

bool isDirectory(WIN32_FIND_DATA* file) {
    /* Windows directory constants */
    return file->dwFileAttributes == 16 || file->dwFileAttributes == 17 || file->dwFileAttributes == 18 || file->dwFileAttributes == 22 || file->dwFileAttributes == 9238;
}

char gopherType(const _file* file) {
    char ret = 'X';
    char ext[4];
    memcpy(&ext, &file->name[strlen(file->name) - 3], 4);
    if (!strstr(file->name, ".")) {
        ret = 'X';
    } else if (!strcmp(ext, "txt") || !strcmp(ext, "doc")) {
        ret = '0';
    } else if (!strcmp(ext, "hqx")) {
        ret = '4';
    } else if (!strcmp(ext, "dos")) {
        ret = '5';
    } else if (!strcmp(ext, "exe")) {
        ret = '9';
    } else if (!strcmp(ext, "gif")) {
        ret = 'g';
    } else if (!strcmp(ext, "jpg") || !strcmp(ext, "png")) {
        ret = 'I';
    } else if (strcmp(ext, "png") == 0) {
        ret = 'I';
    }
    return ret;
}

void gopher(LPCSTR selector, _string response) {
    char wildcardPath[MAX_PATH + 2];
    _file file;
    char line[1 + MAX_PATH + 1 + MAX_PATH + 1 + sizeof("localhost")];
    char type;
    WIN32_FIND_DATA data;
    HANDLE hFind;
    char* failure = "3Error: unknown selector\t\t\r\n.";

    if (strstr(selector, ".\\") || strstr(selector, "..\\")) {
        _log(_READDIR_ERR, ERR, true);
        strcpy(response, failure);
        return;
    }

    if (selector[0] == '\0' || selector[strlen(selector) - 1] == '\\') {  // Directory
        snprintf(wildcardPath, sizeof(wildcardPath), "%s*", (selector[0] == '\0' ? ".\\" : selector));
        if ((hFind = FindFirstFile(wildcardPath, &data)) == INVALID_HANDLE_VALUE) {
            _log(_READDIR_ERR, ERR, true);
            strcpy(response, failure);
            return;
        }
        do {
            if (strcmp(data.cFileName, ".") && strcmp(data.cFileName, "..")) {
                strcpy(file.name, data.cFileName);
                strcpy(file.path, selector);
                strcat(strcpy(file.filePath, selector), file.name);
                type = isDirectory(&data) ? '1' : gopherType(&file);
                snprintf(line, sizeof(line), "%c%s\t%s%s\t%s\r\n", type, file.name, file.filePath, (type == '1' ? "\\" : ""), "localhost");
                strcat(response, line);
            }
        } while (FindNextFile(hFind, &data));
    } else {  // File
        ;
    }
    strcat(response, ".");
}

#else

/*****************************************************************************************************************/
/*                                             UNIX FUNCTIONS                                                    */

/*****************************************************************************************************************/

char gopherType(const _file* file) {
    struct stat fileStat;
    char cmdOut[200] = "";
    FILE* response;
    char* command;
    int bytesRead;
    command = malloc(strlen(file->filePath) + strlen("file \"%s\""));
    sprintf(command, "file \"%s\"", file->filePath);
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

void readDir(const char* path, int sock) {
    DIR* dir;
    struct dirent* entry;
    _file file;
    char line[sizeof(entry->d_name) + sizeof(file.filePath) + strlen("localhost") + 4];
    char* response;
    size_t responseSize;  // Per il punto finale
    char type;

    if ((dir = opendir(path[0] == '\0' ? "./" : path)) == NULL) {
        pthread_exit(NULL);
    }
    response = calloc(1, 1);
    responseSize = 1;  // Per il punto finale
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
            strcat(strcpy(file.filePath, path), entry->d_name);
            type = gopherType(&file);
            snprintf(line, sizeof(line), "%c%s\t%s%s\t%s\r\n", type, entry->d_name, file.filePath, (type == '1' ? "/" : ""), "localhost");
            if ((response = realloc(response, responseSize + strlen(line) + 1)) == NULL) {
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
    closeSocket(sock);
}

void* sendFile(void* sendFileArgs) {
    struct sendFileArgs args;
    void* response;
    args = *(struct sendFileArgs*)sendFileArgs;
    free(sendFileArgs);
    if ((response = calloc(args.size + 3, 1)) == NULL) {
        errorRoutine(&args.dest);
        pthread_exit(NULL);
    };
    memcpy(response, args.src, args.size);
    strcat((char*)response, "\n.");
    send(args.dest, response, args.size + 3, 0);
    free(response);
    closeSocket(args.dest);
    printf("invio terminato\n");
}

void readFile(const char* path, int sock) {
    void* map;
    int fd;
    struct stat statBuf;
    struct sendFileArgs* args;
    pthread_t tid;
    if ((fd = open(path, O_RDONLY)) < 0) {
        pthread_exit(NULL);
    }
    if (fstat(fd, &statBuf) < 0) {
        pthread_exit(NULL);
    }
    if ((map = mmap(NULL, statBuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
        pthread_exit(NULL);
    }
    args = malloc(sizeof(map) + sizeof(size_t) + sizeof(int));
    args->src = map;
    args->size = statBuf.st_size;
    args->dest = sock;
    if (pthread_create(&tid, NULL, sendFile, args)) {
        pthread_exit(NULL);
    }
    pthread_detach(tid);
}

void gopher(const char* selector, int sock) {
    pthread_cleanup_push(errorRoutine, &sock);
    if (strstr(selector, "./") || strstr(selector, "../") || selector[0] == '/' || strstr(selector, "//")) {
        pthread_exit(NULL);
    } else if (selector[0] == '\0' || selector[strlen(selector) - 1] == '/') {  // Directory
        readDir(selector, sock);
    } else {  // File
        readFile(selector, sock);
    }
    pthread_cleanup_pop(0);
}

#endif
