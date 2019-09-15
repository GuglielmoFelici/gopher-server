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

char gopherType(LPSTR file) {
    char ret;
    int extLen = 0;
    LPSTR ext = NULL;
    if (file[0] == '.') {
        return 'H';
    }
    for (int i = strlen(file) - 1; i > 0; i--) {
        if (file[i] == '.') {
            ext = malloc(extLen + 1);
            memcpy(ext, &file[i + 1], extLen);
            ext[extLen] = '\0';
            break;
        }
        extLen++;
    }
    if (ext == NULL) {
        ret = 'X';
    } else if (strcmp(ext, "txt") == 0) {
        ret = '0';
    } else if (strcmp(ext, "hqx") == 0) {
        ret = '4';
    } else if (strcmp(ext, "dos") == 0) {
        ret = '5';
    } else if (strcmp(ext, "exe") == 0) {
        ret = '9';
    } else if (strcmp(ext, "jpg") == 0) {
        ret = 'I';
    }
    free(ext);
    return ret;
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
            snprintf(line, sizeof(line), "%c%s\t%s\t%s\n", gopherType(data.cFileName), data.cFileName, selector, "localhost");
            strcat(response, line);
        }
    } while (FindNextFile(hFind, &data));
}

#else /* Linux functions */

int startup() {}

_string errorString() {
    return strerror(errno);
}

int setNonblocking(_socket s) {
    return fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
}

void readDirectory(_string path, _string response);

#endif