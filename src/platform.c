#include "../headers/platform.h"
#include <stdio.h>
#include "../headers/protocol.h"

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
    // TODO
    // free(logMutex);
    // closesocket(server);
    // closesocket(awakeSelect);
    // CloseHandle(*logMutex);
    // CloseHandle(logEvent);
    // CloseHandle(logPipe);
    // printf("Shutting down...\n");
    // ExitThread(0);
}

int changeCwd(const char *path) {
    return SetCurrentDirectory(path) ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
}

int getCwd(LPSTR dst, size_t size) {
    return GetCurrentDirectory(size, dst) ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
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

int daemon() {
    return PLATFORM_SUCCESS;
}

/*********************************************** FILES  ***************************************************************/

/* Ritorna PLATFORM_SUCCESS se path punta a un file regolare. Altrimenti ritorna PLATFORM_FAILURE con GOPHER_NOT FOUND
   settato se il path non è stato trovato. */
int isFile(LPSTR path) {
    DWORD attr;
    if ((attr = GetFileAttributes(path)) != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        return PLATFORM_SUCCESS;
    } else {
        return GetLastError() == ERROR_FILE_NOT_FOUND ? PLATFORM_FAILURE | GOPHER_NOT_FOUND : PLATFORM_FAILURE;
    }
}

/* Ritorna PLATFORM_SUCCESS se path punta a una directory. Altrimenti ritorna PLATFORM_FAILURE con GOPHER_NOT FOUND
   settato se il path non è stato trovato. */
int isDir(LPCSTR path) {
    DWORD attr;
    if ((attr = GetFileAttributes(path)) != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        return PLATFORM_SUCCESS;
    } else {
        return GetLastError() == ERROR_FILE_NOT_FOUND ? PLATFORM_FAILURE | GOPHER_NOT_FOUND : PLATFORM_FAILURE;
    }
}

/* Mappa un file in memoria */
int getFileMap(LPSTR path, file_mapping_t *mapData) {
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
        return PLATFORM_SUCCESS;
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
    return PLATFORM_SUCCESS;
ON_ERROR:
    if (file != INVALID_HANDLE_VALUE) {
        CloseHandle(file);
    }
    if (map != INVALID_HANDLE_VALUE) {
        CloseHandle(map);
    }
    return PLATFORM_FAILURE;
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
            return GetLastError() == ERROR_FILE_NOT_FOUND ? PLATFORM_FAILURE | GOPHER_NOT_FOUND : PLATFORM_FAILURE;
        }
    } else {
        if (!FindNextFile(*dir, &data)) {
            return GetLastError() == ERROR_NO_MORE_FILES ? PLATFORM_FAILURE | GOPHER_END_OF_DIR : PLATFORM_FAILURE;
        }
    }
    strncpy(name, data.cFileName, nameSize);
    return PLATFORM_SUCCESS;
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
    return chdir(path) < 0 ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
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

int daemon() {
    int pid;
    // pid = fork();
    pid = 0;
    if (pid < 0) {
        return PLATFORM_FAILURE;
    } else if (pid > 0) {
        exit(0);
    } else {
        sigset_t set;
        // if (setsid() < 0) {
        //     return PLATFORM_FAILURE;
        // }
        if (sigemptyset(&set) < 0) {
            return PLATFORM_FAILURE;
        }
        if (sigaddset(&set, SIGHUP) < 0) {
            return PLATFORM_FAILURE;
        }
        if (sigprocmask(SIG_BLOCK, &set, NULL) < 0) {
            return PLATFORM_FAILURE;
        }
        // pid = fork();
        pid = 0;
        if (pid < 0) {
            return PLATFORM_FAILURE;
        } else if (pid > 0) {
            exit(0);
        } else {
            umask(S_IRUSR & S_IWUSR & S_IRGRP & S_IWGRP);
            if (sigprocmask(SIG_UNBLOCK, &set, NULL) < 0) {
                return PLATFORM_FAILURE;
            }
            int devNull;
            // TODO eventualmente usare syslog
            if ((devNull = open("/dev/null", O_RDWR)) < 0) {
                return PLATFORM_FAILURE;
            }
            if (dup2(serverStdIn, STDIN_FILENO) < 0)
                ;  //|| dup2(devNull, STDOUT_FILENO) < 0 || dup2(devNull, STDERR_FILENO) < 0) {
            // return PLATFORM_FAILURE;
            // }
            return close(devNull) >= 0 ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
        }
    }
    return 0;
}

/*********************************************** FILES ****************************************************************/

/* Ritorna PLATFORM_SUCCESS se path punta a un file regolare. Altrimenti ritorna PLATFORM_FAILURE con GOPHER_NOT FOUND
   settato se il path non è stato trovato. */
int isFile(char *path) {
    struct stat statbuf;
    if (stat(path, &statbuf) != 0) {
        return errno == ENOENT ? PLATFORM_FAILURE | GOPHER_NOT_FOUND : PLATFORM_FAILURE;
    }
    return S_ISREG(statbuf.st_mode) ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
}

/* Ritorna PLATFORM_SUCCESS se path punta a una directory. Altrimenti ritorna PLATFORM_FAILURE con GOPHER_NOT FOUND
   settato se il path non è stato trovato. */
int isDir(const char *path) {
    struct stat statbuf;
    if (stat(path, &statbuf) != 0) {
        return errno == ENOENT ? PLATFORM_FAILURE | GOPHER_NOT_FOUND : PLATFORM_FAILURE;
    }
    return S_ISDIR(statbuf.st_mode) ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
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
        return PLATFORM_FAILURE;
    }
    mapData->view = map;
    mapData->size = statBuf.st_size;
    return PLATFORM_SUCCESS;
}

/* 
    Legge la prossima entry nella directory e ne mette il nome in name. 
    Se *dir è NULL, apre la directory contenuta in path. Se *dir non è NULL, path può essere NULL
*/
int iterateDir(const char *path, DIR **dir, char *name, size_t nameSize) {
    struct dirent *entry;
    if (*dir == NULL) {
        if ((*dir = opendir(path)) == NULL) {
            return errno == ENOENT ? PLATFORM_FAILURE | GOPHER_NOT_FOUND : PLATFORM_FAILURE;
        }
    }
    entry = readdir(*dir);
    if (entry == NULL) {
        return errno == EBADF ? PLATFORM_FAILURE : PLATFORM_FAILURE | GOPHER_END_OF_DIR;
    }
    strncpy(name, entry->d_name, nameSize);
    return PLATFORM_SUCCESS;
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

int sendAll(socket_t s, char *data, int length) {
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
