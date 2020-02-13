#include "../headers/platform.h"
#include <stdio.h>
#include "../headers/protocol.h"

#if defined(_WIN32)

/*****************************************************************************************************************/
/*                                             WINDOWS FUNCTIONS                                                 */

/*****************************************************************************************************************/

/************************************************** UTILS ********************************************************/

void errorString(LPSTR error, size_t size) {
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  error, size, NULL);
}

int getCwd(LPSTR dst, size_t size) {
    return GetCurrentDirectory(size, dst) ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
}

int changeCwd(LPCSTR path) {
    return SetCurrentDirectory(path) ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
}

void logMessage(cstring_t message, int level) {
    string_t lvl = "";
    switch (level) {
        case LOG_DEBUG:
            lvl = "DEBUG";
            break;
        case LOG_INFO:
            lvl = "INFO";
            break;
        case LOG_WARNING:
            lvl = "WARNING";
            break;
        case LOG_ERR:
            lvl = "ERROR";
    }
    fprintf(stderr, "%s - %s\n", lvl, message);  // TODO system error
}

/********************************************** SOCKETS *************************************************************/

/* Ritorna l'ultimo codice di errore relativo alle chiamate WSA */
int sockErr() {
    return WSAGetLastError();
}

int closeSocket(SOCKET s) {
    return closesocket(s) == 0 ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
}

LPCSTR inetNtoa(const struct in_addr *addr, void *dst, size_t size) {
    return strncpy(dst, inet_ntoa(*addr), size);
}

/*********************************************** THREADS & PROCESSES ***************************************************************/

int detachThread(HANDLE tHandle) {
    return CloseHandle(tHandle) ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
}

int startThread(HANDLE *tid, LPTHREAD_START_ROUTINE routine, void *args) {
    *tid = CreateThread(NULL, 0, routine, args, 0, NULL);
    return tid ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
}

int daemonize() {
    return PLATFORM_SUCCESS;
}

/*********************************************** FILES  ***************************************************************/

int fileAttributes(LPCSTR path) {
    DWORD attr = GetFileAttributes(path);
    if (INVALID_FILE_ATTRIBUTES == attr) {
        return GetLastError() == ERROR_FILE_NOT_FOUND ? PLATFORM_FILE_ERR | PLATFORM_NOT_FOUND : PLATFORM_FILE_ERR;
    }
    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        return PLATFORM_ISDIR;
    } else {
        return PLATFORM_ISFILE;
    }
}

/* Mappa un file in memoria */
int getFileMap(LPCSTR path, file_mapping_t *mapData) {
    HANDLE file = NULL, map = NULL;
    LPVOID view = NULL;
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
    if (!LockFileEx(file, LOCKFILE_EXCLUSIVE_LOCK, 0, fileSize.LowPart, fileSize.HighPart, &ovlp)) {
        goto ON_ERROR;
    }
    if (NULL == (map = CreateFileMapping(file, NULL, PAGE_READONLY, 0, 0, NULL))) {
        UnlockFileEx(file, 0, fileSize.LowPart, fileSize.HighPart, &ovlp);
        goto ON_ERROR;
    }
    if (
        !UnlockFileEx(file, 0, fileSize.LowPart, fileSize.HighPart, &ovlp) ||
        !CloseHandle(file) ||
        NULL == (view = MapViewOfFile(map, FILE_MAP_READ, 0, 0, 0)) ||
        !CloseHandle(map)) {
        goto ON_ERROR;
    }
    mapData->view = view;
    mapData->size = fileSize.LowPart;
    return PLATFORM_SUCCESS;
ON_ERROR:
    if (file) {
        CloseHandle(file);
    }
    if (map) {
        CloseHandle(map);
    }
    if (view) {
        UnmapViewOfFile(view);
    }
    return PLATFORM_FAILURE;
}

/* 
    Legge la prossima entry nella directory. 
    Se *dir è NULL, apre la directory contenuta in path. Se *dir non è NULL, path può essere NULL 
*/
int iterateDir(LPCSTR path, HANDLE *dir, LPSTR name, size_t nameSize) {
    WIN32_FIND_DATA data;
    if (*dir == NULL) {
        char dirPath[MAX_NAME + 2];
        snprintf(dirPath, sizeof(dirPath), "%s/*", path);
        if ((*dir = FindFirstFile(dirPath, &data)) == INVALID_HANDLE_VALUE) {
            return GetLastError() == ERROR_FILE_NOT_FOUND ? PLATFORM_FAILURE | PLATFORM_NOT_FOUND : PLATFORM_FAILURE;
        }
    } else {
        if (!FindNextFile(*dir, &data)) {
            return GetLastError() == ERROR_NO_MORE_FILES ? PLATFORM_FAILURE | PLATFORM_END_OF_DIR : PLATFORM_FAILURE;
        }
    }
    bool isCurrent = strcmp(path, ".") == 0;
    snprintf(name, nameSize, "%s%s%s", isCurrent ? "" : path, isCurrent ? "" : "/", data.cFileName);
    return PLATFORM_SUCCESS;
}

int closeDir(HANDLE dir) {
    return FindClose(dir) ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
}

int unmapMem(void *addr, size_t len) {
    return UnmapViewOfFile(addr) ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
}

#else

/*****************************************************************************************************************/
/*                                             LINUX FUNCTIONS                                                    */

/*****************************************************************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/************************************************** UTILS ********************************************************/

void errorString(char *error, size_t size) {
    snprintf(error, size, "%s", strerror(errno));
}

int getCwd(char *dst, size_t size) {
    return getcwd(dst, size) ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
}

int changeCwd(const char *path) {
    return chdir(path) >= 0 ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
}

void logMessage(cstring_t message, int level) {
    syslog(level, message);
}

/********************************************** SOCKETS *************************************************************/

int sockErr() {
    return errno;
}

int closeSocket(int s) {
    return close(s) == 0 ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
}

const char *inetNtoa(const struct in_addr *addr, void *dst, size_t size) {
    return inet_ntop(AF_INET, &addr->s_addr, dst, size);
}

/*********************************************** THREADS & PROCESSES ***************************************************************/

int detachThread(pthread_t tid) {
    return pthread_detach(tid) == 0 ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
}

int startThread(pthread_t *tid, LPTHREAD_START_ROUTINE routine, void *args) {
    return pthread_create(tid, NULL, routine, args) == 0 ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
}

int daemonize() {
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
            if (dup2(devNull, STDIN_FILENO) < 0)
                ;  //|| dup2(devNull, STDOUT_FILENO) < 0 || dup2(devNull, STDERR_FILENO) < 0) {
            // return PLATFORM_FAILURE;
            // }
            return close(devNull) >= 0 ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
        }
    }
}

/*********************************************** FILES ****************************************************************/

int fileAttributes(cstring_t path) {
    struct stat statbuf;
    if (stat(path, &statbuf) != 0) {
        return errno == ENOENT ? PLATFORM_FILE_ERR | PLATFORM_NOT_FOUND : PLATFORM_FILE_ERR;
        ;
    }
    if (S_ISREG(statbuf.st_mode)) {
        return PLATFORM_ISFILE;
    } else if (S_ISDIR(statbuf.st_mode)) {
        return PLATFORM_ISDIR;
    } else {
        return PLATFORM_FILE_ERR;
    }
}

/* Ritorna PLATFORM_SUCCESS se path punta a un file regolare. Altrimenti ritorna PLATFORM_FAILURE con GOPHER_NOT FOUND
   settato se il path non è stato trovato. */
bool isFile(const char *path, int *error) {
    struct stat statbuf;
    if (stat(path, &statbuf) != 0) {
        if (error) {
            *error = errno == ENOENT ? PLATFORM_FAILURE | PLATFORM_NOT_FOUND : PLATFORM_FAILURE;
        }
        return false;
    }
    return S_ISREG(statbuf.st_mode);
}

/* Ritorna PLATFORM_SUCCESS se path punta a una directory. Altrimenti ritorna PLATFORM_FAILURE con GOPHER_NOT FOUND
   settato se il path non è stato trovato. */
bool isDir(const char *path, int *error) {
    struct stat statbuf;
    if (stat(path, &statbuf) != 0) {
        *error = errno == ENOENT ? PLATFORM_NOT_FOUND : PLATFORM_FAILURE;
        return false;
    }
    return S_ISDIR(statbuf.st_mode);
}

int getFileMap(const char *path, file_mapping_t *mapData) {
    void *map;
    int fd;
    struct stat statBuf;
    struct sendFileArgs *args;
    pthread_t tid;
    if ((fd = open(path, O_RDONLY)) < 0) {
        return PLATFORM_FAILURE;
    }
    if (flock(fd, LOCK_EX) < 0) {  // Verificare rispetto a lockf e fcntl
        return PLATFORM_FAILURE;
    }
    if (fstat(fd, &statBuf) < 0) {
        flock(fd, LOCK_UN);
        return PLATFORM_FAILURE;
    }
    if (MAP_FAILED == (map = mmap(NULL, statBuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0))) {
        flock(fd, LOCK_UN);
        return PLATFORM_FAILURE;
    }
    if (flock(fd, LOCK_UN) < 0 ||
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
        if (NULL == (*dir = opendir(path))) {
            return errno == ENOENT ? PLATFORM_FAILURE | PLATFORM_NOT_FOUND : PLATFORM_FAILURE;
        }
    }
    entry = readdir(*dir);
    if (entry == NULL) {
        return errno == EBADF ? PLATFORM_FAILURE : PLATFORM_FAILURE | PLATFORM_END_OF_DIR;
    }
    bool isCurrent = strcmp(path, ".") == 0;
    snprintf(name, nameSize, "%s%s%s", isCurrent ? "" : path, isCurrent ? "" : "/", entry->d_name);
    return PLATFORM_SUCCESS;
}

int closeDir(DIR *dir) {
    return closedir(dir) == 0 ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
}

int unmapMem(void *addr, size_t len) {
    return munmap(addr, len) == 0 ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
}

#endif

/*****************************************************************************************************************/
/*                                             COMMON FUNCTIONS                                                 */

/*****************************************************************************************************************/

bool endsWith(cstring_t str1, cstring_t str2) {
    return strcmp(str1 + (strlen(str1) - strlen(str2)), str2) == 0;
}

int sendAll(socket_t s, cstring_t data, int length) {
    int count = 0, sent = 0;
    while (count < length) {
        int sent = send(s, data + count, length, 0);
        if (sent == SOCKET_ERROR) {
            return PLATFORM_FAILURE;
        }
        count += sent;
        length -= sent;
    }
    return PLATFORM_SUCCESS;
}

void debugPause(cstring_t message) {
    printf("%s\n", message);
    while ('g' != getchar())
        ;
}