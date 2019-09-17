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

bool isDirectory(WIN32_FIND_DATA *data) {
    /* Windows directory constants */
    return data->dwFileAttributes == 16 || data->dwFileAttributes == 17 || data->dwFileAttributes == 18 || data->dwFileAttributes == 22 || data->dwFileAttributes == 9238;
}

char gopherType(WIN32_FIND_DATA *data) {
    char *file = data->cFileName;
    char ret = 'X';
    int extLen = 0;
    char ext[10] = "";
    if (file[0] == '.') {
        ret = 'H';
    } else {
        for (int i = strlen(file) - 1; i > 0; i--) {
            if (file[i] == '.') {
                memcpy(ext, &file[i + 1], extLen);
                ext[extLen] = '\0';
                break;
            }
            extLen++;
        }
        if (ext == "") {
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
    }
    return ret;
}

void readDirectory(_string path, _string response) {
    char wildcardPath[MAX_PATH];
    char selector[MAX_PATH];
    char line[1 + MAX_PATH + MAX_PATH];
    char type;
    WIN32_FIND_DATA data;
    HANDLE hFind;

    snprintf(wildcardPath, sizeof(wildcardPath), "%s\\*", path);
    if ((hFind = FindFirstFile(wildcardPath, &data)) == INVALID_HANDLE_VALUE) {
        err(_READDIR_ERR, ERR, true, -1);
    }
    do {
        /* Lunghezza della riga: 1+filename+selector+?host? */
        snprintf(selector, sizeof(selector), "%s\\%s", path, data.cFileName);
        if (selector[strlen(selector) - 1] != '.') {
            type = isDirectory(&data) ? '1' : gopherType(&data);
            snprintf(line, sizeof(line), "%c%s\t%s\t%s\n", type, data.cFileName, selector, "localhost");
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

// TODO
bool isDirectory(struct stat* data) {
    return S_ISDIR(data->st_mode);
}

// TODO
char gopherType(struct stat* data) {
}

// TODO
void readDirectory(char* path, char* response){

};

#endif
