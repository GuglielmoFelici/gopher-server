#include "includes/platform.h"

#if defined(_WIN32) /* Windows functions */

int startup() {
    WORD versionWanted = MAKEWORD(1, 1);
    WSADATA wsaData;
    return WSAStartup(versionWanted, &wsaData);
}

/* Took from  https://docs.microsoft.com/it-it/windows/win32/debug/retrieving-the-last-error-code*/
_string errorString() {
    LPVOID lpMsgBuf;
    DWORD dw = GetLastError();
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf,
        0, NULL);
    return (LPTSTR)lpMsgBuf;
}

int setNonblocking(_socket s) {
    unsigned long blocking = 1;
    return ioctlsocket(s, FIONBIO, &blocking);
}

bool isDirectory(WIN32_FIND_DATA *file) {
    /* Windows directory constants */
    return file->dwFileAttributes == 16 || file->dwFileAttributes == 17 || file->dwFileAttributes == 18 || file->dwFileAttributes == 22 || file->dwFileAttributes == 9238;
}

char gopherType(LPCSTR file) {
    char ret = 'X';
    char ext[4];
    memcpy(&ext, &file[strlen(file) - 3], 4);
    if (!strstr(file, ".")) {
        ret = 'X';
    } else if (strcmp(ext, "txt") == 0) {
        ret = '0';
    } else if (strcmp(ext, "hqx") == 0) {
        ret = '4';
    } else if (strcmp(ext, "dos") == 0) {
        ret = '5';
    } else if (strcmp(ext, "exe") == 0) {
        ret = '9';
    } else if (strcmp(ext, "gif") == 0) {
        ret = 'g';
    } else if (strcmp(ext, "jpg") == 0) {
        ret = 'I';
    } else if (strcmp(ext, "png") == 0) {
        ret = 'I';
    }
    return ret;
}

void readDirectory(LPCSTR path, _string response) {
    char wildcardPath[MAX_PATH];
    LPSTR name;
    char line[1 + MAX_PATH + 1 + MAX_PATH + 1 + sizeof("localhost")];
    char type;
    WIN32_FIND_DATA data;
    HANDLE hFind;

    snprintf(wildcardPath, sizeof(wildcardPath), "%s\\*", path);
    if ((hFind = FindFirstFile(wildcardPath, &data)) == INVALID_HANDLE_VALUE) {
        err(_READDIR_ERR, ERR, true, -1);
    }
    do {
        name = data.cFileName;
        /* TODO Lunghezza della riga: 1+filename+selector+?host? */
        //snprintf(selector, sizeof(selector), "%s\\%s", path, name);
        if (name[strlen(name) - 1] != '.') {
            type = isDirectory(&data) ? '1' : gopherType(name);
            snprintf(line, sizeof(line), "%c%s\t%s\\%s\t%s\n", type, name, path, name, "localhost");
            strcat(response, line);
        }
    } while (FindNextFile(hFind, &data));
}

#else /* Linux functions */

int startup() {}

char* errorString() {
    return strerror(errno);
}

int setNonblocking(_socket s) {
    return fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
}

bool isDirectory(struct stat* file) {
    return S_ISDIR(file->st_mode);
}

char gopherType(_file* file) {
    struct stat fileStat;
    char buffer[600] = "";
    FILE* response;
    char command[FILENAME_MAX + 7];
    sprintf(command, "file \"%s\"", file->filePath);
    if ((response = popen(command, "r")) == NULL) {
        err(_EXEC_ERR, ERR, true, -1);
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

void readDirectory(const char* path, char* response) {
    DIR* dir;
    struct dirent* entry;
    struct stat fileStat;
    _file file;
    char line[1 + FILENAME_MAX + 1 + FILENAME_MAX + 1 + sizeof("localhost")];

    if ((dir = opendir(path)) == NULL) {
        err(_READDIR_ERR, ERR, true, -1);
    }
    while ((entry = readdir(dir)) != NULL) {
        strcpy(file.name, entry->d_name);
        strcpy(file.path, path);
        strcpy(file.filePath, path);
        strcat(strcat(file.filePath, "/"), file.name);
        if (file.name[strlen(file.name) - 1] != '.') {
            sprintf(line, "%c%s\t%s\t%s\n", gopherType(&file), file.name, file.filePath, "localhost");
            strcat(response, line);
        }
    }
    strcat(response, ".");
};

#endif
