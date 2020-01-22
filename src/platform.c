#include "../headers/platform.h"
#include "../headers/protocol.h"
#include "headers/gopher.h"

#if defined(_WIN32)

/*****************************************************************************************************************/
/*                                             WINDOWS FUNCTIONS                                                 */

/*****************************************************************************************************************/

/************************************************** UTILS ********************************************************/

void errorString(char *error, size_t size) {
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  error, size, NULL);
}

/* Termina graziosamente il logger, poi termina il server. */
void _shutdown(SOCKET server) {
    free(logMutex);
    closesocket(server);
    closesocket(awakeSelect);
    CloseHandle(*logMutex);
    CloseHandle(logEvent);
    CloseHandle(logPipe);
    printf("Shutting down...\n");
    ExitThread(0);
}

int changeCwd(const char *path) {
    return SetCurrentDirectory(path) ? GOPHER_SUCCESS : GOPHER_FAILURE;
}

int getCwd(LPSTR dst, size_t size) {
    return GetCurrentDirectory(size, dst) ? GOPHER_SUCCESS : GOPHER_FAILURE;
}

/********************************************** SOCKETS *************************************************************/

/* Ritorna l'ultimo codice di errore relativo alle chiamate WSA */
int sockErr() {
    return WSAGetLastError();
}

int closeSocket(SOCKET s) {
    return closesocket(s);
}

const char *inetNtoa(struct in_addr *addr, void *dst, size_t size) {
    return strncpy(dst, inet_ntoa(*addr), size);
}

/*********************************************** THREADS & PROCESSES ***************************************************************/

bool detachThread(HANDLE tHandle) {
    CloseHandle(tHandle);
}

int _createThread(HANDLE *tid, LPTHREAD_START_ROUTINE routine, void *args) {
    *tid = CreateThread(NULL, 0, routine, args, 0, NULL);
    if (tid != NULL) {
        return 0;
    }
    return -1;
}

/*********************************************** FILES  ***************************************************************/

/* Ritorna GOPHER_SUCCESS se path punta a un file regolare. Altrimenti ritorna GOPHER_FAILURE con GOPHER_NOT FOUND
   settato se il path non è stato trovato. */
int isFile(LPSTR path) {
    DWORD attr;
    if ((attr = GetFileAttributes(path)) != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        return GOPHER_SUCCESS;
    } else {
        return GetLastError() == ERROR_FILE_NOT_FOUND ? GOPHER_FAILURE | GOPHER_NOT_FOUND : GOPHER_FAILURE;
    }
}

/* Ritorna GOPHER_SUCCESS se path punta a una directory. Altrimenti ritorna GOPHER_FAILURE con GOPHER_NOT FOUND
   settato se il path non è stato trovato. */
int isDir(LPCSTR path) {
    DWORD attr;
    if ((attr = GetFileAttributes(path)) != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        return GOPHER_SUCCESS;
    } else {
        return GetLastError() == ERROR_FILE_NOT_FOUND ? GOPHER_FAILURE | GOPHER_NOT_FOUND : GOPHER_FAILURE;
    }
}

/* Mappa un file in memoria */
int getFileMap(LPSTR path, struct fileMappingData *mapData) {
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

/* 
    Legge la prossima entry nella directory. 
    Se *dir è NULL, apre la directory contenuta in path. Se *dir non è NULL, path può essere NULL 
*/
int iterateDir(const char *path, HANDLE *dir, LPSTR name, size_t nameSize) {
    WIN32_FIND_DATA data;
    if (*dir == NULL) {
        char dirPath[MAX_NAME + 1];
        snprintf(dirPath, MAX_NAME, "%s*", path);
        if ((*dir = FindFirstFile(dirPath, &data)) == INVALID_HANDLE_VALUE) {
            return GetLastError() == ERROR_FILE_NOT_FOUND ? GOPHER_FAILURE | GOPHER_NOT_FOUND : GOPHER_FAILURE;
        }
    } else {
        if (!FindNextFile(*dir, &data)) {
            return GetLastError() == ERROR_NO_MORE_FILES ? GOPHER_FAILURE | GOPHER_END_OF_DIR : GOPHER_FAILURE;
        }
    }
    strncpy(name, data.cFileName, nameSize);
    return GOPHER_SUCCESS;
}

int closeDir(HANDLE dir) {
    return FindClose(dir);
}

int unmapMem(void *addr, size_t len) {
    return UnmapViewOfFile(addr);
}

#else

/*****************************************************************************************************************/
/*                                             LINUX FUNCTIONS                                                    */

/*****************************************************************************************************************/

/************************************************** UTILS ********************************************************/

/* Termina il logger ed esce. */
void _shutdown(int server) {
    kill(loggerPid, SIGINT);
    close(logPipe);
    close(server);
    pthread_exit(0);
}

void errorString(char *error, size_t size) {
    snprintf(error, size, "%s", strerror(errno));
}

int changeCwd(const char *path) {
    return chdir(path) < 0 ? GOPHER_SUCCESS : GOPHER_FAILURE;
}

/********************************************** SOCKETS *************************************************************/

int sockErr() {
    return errno;
}

int closeSocket(int s) {
    return close(s);
}

const char *inetNtoa(struct in_addr *addr, void *dst, size_t size) {
    return inet_ntop(AF_INET, &addr->s_addr, dst, size);
}

/*********************************************** THREADS & PROCESSES ***************************************************************/

bool detachThread(pthread_t tid) {
    return pthread_detach(tid) >= 0;
}

int _createThread(pthread_t *tid, LPTHREAD_START_ROUTINE routine, void *args) {
    return pthread_create(tid, NULL, routine, args);
}

/*********************************************** FILES ****************************************************************/

/* Ritorna GOPHER_SUCCESS se path punta a un file regolare. Altrimenti ritorna GOPHER_FAILURE con GOPHER_NOT FOUND
   settato se il path non è stato trovato. */
int isFile(char *path) {
    struct stat statbuf;
    if (stat(path, &statbuf) != 0) {
        return errno == ENOENT ? GOPHER_FAILURE | GOPHER_NOT_FOUND : GOPHER_FAILURE;
    }
    return S_ISREG(statbuf.st_mode) ? GOPHER_SUCCESS : GOPHER_FAILURE;
}

/* Ritorna GOPHER_SUCCESS se path punta a una directory. Altrimenti ritorna GOPHER_FAILURE con GOPHER_NOT FOUND
   settato se il path non è stato trovato. */
int isDir(const char *path) {
    struct stat statbuf;
    if (stat(path, &statbuf) != 0) {
        return errno == ENOENT ? GOPHER_FAILURE | GOPHER_NOT_FOUND : GOPHER_FAILURE;
    }
    return S_ISDIR(statbuf.st_mode) ? GOPHER_SUCCESS : GOPHER_FAILURE;
}

int getFileMap(char *path, struct fileMappingData *mapData) {
    void *map;
    int fd;
    struct stat statBuf;
    struct sendFileArgs *args;
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

/* 
    Legge la prossima entry nella directory e ne mette il nome in name. 
    Se *dir è NULL, apre la directory contenuta in path. Se *dir non è NULL, path può essere NULL
*/
int iterateDir(const char *path, DIR **dir, char *name, size_t nameSize) {
    struct dirent *entry;
    if (*dir == NULL) {
        if ((*dir = opendir(path)) == NULL) {
            return errno == ENOENT ? GOPHER_FAILURE | GOPHER_NOT_FOUND : GOPHER_FAILURE;
        }
    }
    entry = readdir(*dir);
    if (entry == NULL) {
        return errno == EBADF ? GOPHER_FAILURE : GOPHER_FAILURE | GOPHER_END_OF_DIR;
    }
    strncpy(name, entry->d_name, nameSize);
    return GOPHER_SUCCESS;
}

int closeDir(DIR *dir) {
    return closedir(dir);
}

int unmapMem(void *addr, size_t len) {
    return munmap(addr, len);
}

/*********************************************** LOGGER ***************************************************************/

#endif

/*****************************************************************************************************************/
/*                                             COMMON FUNCTIONS                                                 */

/*****************************************************************************************************************/

bool endsWith(char *str1, char *str2) {
    return strcmp(str1 + (strlen(str1) - strlen(str2)), str2) == 0;
}

int sendAll(_socket s, char *data, int length) {
    int count = 0, sent = 0;
    while (count < length) {
        int sent = send(s, data + count, length, 0);
        if (sent == SOCKET_ERROR) {
            return SOCKET_ERROR;
        }
        count += sent;
        length -= sent;
    }
    return 0;
}

/* Stampa l'errore su stderr ed esce */
void _err(_cstring message, bool stderror, int code) {
    char error[MAX_ERROR_SIZE] = "";
    if (stderror) {
        errorString(error, sizeof(error));
    }
    fprintf(stderr, "%s - %s\n", message, error);
    exit(code);
}

/* Stampa l'errore su stderr */
void _logErr(_cstring message) {
    char buf[MAX_ERROR_SIZE];
    errorString(buf, MAX_ERROR_SIZE);
    fprintf(stderr, "%s\nSystem error message: %s\n", message, buf);
}
