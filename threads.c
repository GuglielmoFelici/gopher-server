#include "includes/threads.h"
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

void gopherResponse(LPCSTR selector, _string response) {
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

bool isDirectory(struct stat* file) {
    return S_ISDIR(file->st_mode);
}

char gopherType(const _file* file) {
    struct stat fileStat;
    char buffer[600] = "";
    FILE* response;
    char command[FILENAME_MAX + 7];
    sprintf(command, "file \"%s\"", file->filePath);
    if ((response = popen(command, "r")) == NULL) {
        err(_EXEC_ERR, ERR, true, errno);
    }
    fread(buffer, 1, 199, response);
    if (strstr(buffer, "executable")) {
        return '9';
    } else if (strstr(buffer, "image")) {
        return 'I';
    } else if (strstr(buffer, "directory")) {
        return '1';
    } else if (strstr(buffer, "GIF")) {
        return 'G';
    } else if (strstr(buffer, "text")) {
        return '0';
    }
}

void gopherResponse(const char* selector, char* response) {
    struct stat fileStat;
    char* err = "3Error: error reading file, check the selector\t\t\r\n.";

    if (strstr(selector, "./") || strstr(selector, "../") || selector[0] == '/') {
        _log(_READDIR_ERR, ERR, true);
        strcpy(response, err);
        return;
    }
    if (selector[0] == '\0' || selector[strlen(selector) - 1] == '/') {  // Directory
        DIR* dir;
        struct dirent* entry;
        _file file;
        char line[1 + FILENAME_MAX + 1 + FILENAME_MAX + 1 + sizeof("localhost")];
        char type;
        if ((dir = opendir(selector[0] == '\0' ? "./" : selector)) == NULL) {
            _log(_READDIR_ERR, ERR, true);
            strcpy(response, err);
            return;
        }
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
                strcat(strcpy(file.filePath, selector), entry->d_name);
                char type = gopherType(&file);
                snprintf(line, sizeof(line), "%c%s\t%s%s\t%s\r\n", type, entry->d_name, file.filePath, (type == '1' ? "/" : ""), "localhost");
                strcat(response, line);
            }
        }
    } else {  // File
    }
    strcat(response, ".");
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

void* task(void* args) {
    char message[256];
    char response[10000];
    printf("starting thread\n");
    _socket sock = *(_socket*)args;
    recv(sock, message, sizeof(message), 0);
    trimEnding(message);
    printf("received: %s;\n", message);
    gopherResponse(message, response);
    printf("%s\n", response);
    closeSocket(sock);
    free(args);
    printf("Closing thread...\n");
}