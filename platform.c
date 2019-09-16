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

bool isDirectory(LPSTR path) {
    return GetFileAttributes(path) == FILE_ATTRIBUTE_DIRECTORY;
}

char gopherType(LPSTR ext) {
    if (ext == 0) {
        return 'X';
    } else if (strcmp(ext, "txt") == 0) {
        return '0';
    } else if (strcmp(ext, "hqx") == 0) {
        return '4';
    } else if (strcmp(ext, "dos") == 0) {
        return '5';
    } else if (strcmp(ext, "exe") == 0) {
        return '9';
    } else if (strcmp(ext, "gif") == 0) {
        return 'g';
    } else if (strcmp(ext, "jpg") == 0) {
        return 'I';
    } else if (strcmp(ext, "png") == 0) {
        return 'I';
    }
    return 'X';
}

void readDirectory(_string path, _string response) {
    char wildcardPath[MAX_PATH];
    char selector[MAX_PATH];
    char line[1 + MAX_PATH + MAX_PATH];
    int type;
    WIN32_FIND_DATA data;
    HANDLE hFind;

    snprintf(wildcardPath, sizeof(wildcardPath), "%s\\*", path);
    if ((hFind = FindFirstFile(wildcardPath, &data)) == INVALID_HANDLE_VALUE) {
        err(_READDIR_ERR, ERR, true, -1);
    }
    do {
        /* Lunghezza della riga: 1+filename+selector+?host? */
        snprintf(selector, sizeof(selector), "%s\\%s", path, data.cFileName);
        // TODO directory type
        if (selector[strlen(selector) - 1] != '.') {
            snprintf(line, sizeof(line), "%c%s\t%s\t%s\n", parseFileName(data.cFileName), data.cFileName, selector, "localhost");
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
bool isDirectory(char* path) {
}

// TODO
char gopherType(char* ext) {
    if (ext == 0) {
        return 'X';
    } else if (strcmp(ext, "txt") == 0) {
        return '0';
    } else if (strcmp(ext, "hqx") == 0) {
        return '4';
    } else if (strcmp(ext, "dos") == 0) {
        return '5';
    } else if (strcmp(ext, "exe") == 0) {
        return '9';
    } else if (strcmp(ext, "gif") == 0) {
        return 'g';
    } else if (strcmp(ext, "jpg") == 0) {
        return 'I';
    } else if (strcmp(ext, "png") == 0) {
        return 'I';
    }
    return 'X';
}

void readDirectory(char* path, char* response);

#endif

char parseFileName(_string file) {
    char ret;
    int extLen = 0;
    char ext[10] = 0;
    if (file[0] == '.') {
        ret = 'H';
    } else if (isDirectory(file)) {
        ret = '1';
    } else {
        for (int i = strlen(file) - 1; i > 0; i--) {
            if (file[i] == '.') {
                memcpy(ext, &file[i + 1], extLen);
                ext[extLen] = '\0';
                break;
            }
            extLen++;
        }
        ret = gopherType(ext);
    }
    return ret;
}