#include "../headers/platform.h"

#include <stdio.h>
#include <string.h>

#include "../headers/globals.h"

int debugLevel = DBG_WARN;

bool endsWith(cstring_t str1, cstring_t str2) {
    return strcmp(str1 + (strlen(str1) - strlen(str2)), str2) == 0;
}

int sendAll(socket_t s, const void* data, file_size_t length) {
    size_t count = 0, sent = 0;
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

/*****************************************************************************************************************/
/*                                             WINDOWS FUNCTIONS                                                 */

/*****************************************************************************************************************/

#if defined(_WIN32)

#define LO32(_qw) ((DWORD)(_qw))
#define HI32(_qw) ((DWORD)(((_qw) >> 32) & 0xffffffff))

/************************************************** UTILS ********************************************************/

int getCwd(LPSTR dst, size_t size) {
    return GetCurrentDirectory(size, dst) ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
}

int changeCwd(LPCSTR path) {
    return SetCurrentDirectory(path) ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
}

void debugMessage(cstring_t message, int level) {
    if (debugLevel < level) {
        return;
    }
    FILE* where = stdout;
    string_t lvl = "";
    switch (level) {
        case DBG_DEBUG:
            lvl = "DEBUG";
            break;
        case DBG_INFO:
            lvl = "INFO";
            break;
        case DBG_WARN:
            where = stderr;
            lvl = "WARNING";
            break;
        case DBG_ERR:
            where = stderr;
            lvl = "ERROR";
    }
    fprintf(where, "%s - %s", lvl, message);
    if (level == DBG_ERR) {
        fprintf(stderr, " - %d", GetLastError());
    }
    fprintf(stderr, "\n");
}

int getWindowsHelpersPaths() {
    if (NULL == (winLoggerPath = getRealPath(WINLOGGER_PATH, NULL, false))) {
        return PLATFORM_FAILURE;
    }
    return (winHelperPath = getRealPath(WINHELPER_PATH, NULL, false)) ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
}

/********************************************** SOCKETS *************************************************************/

/* Ritorna l'ultimo codice di errore relativo alle chiamate WSA */
int sockErr() {
    return WSAGetLastError();
}

int closeSocket(SOCKET s) {
    return closesocket(s) == 0 ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
}

LPCSTR inetNtoa(const struct in_addr* addr, void* dst, size_t size) {
    return strncpy(dst, inet_ntoa(*addr), size);
}

/*********************************************** THREADS & PROCESSES ***************************************************************/

int startThread(HANDLE* thrd, LPTHREAD_START_ROUTINE routine, void* args) {
    *thrd = CreateThread(NULL, 0, routine, args, 0, NULL);
    return thrd ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
}

int joinThread(HANDLE thrd) {
    return WaitForSingleObject(thrd, INFINITE) == WAIT_FAILED ? PLATFORM_FAILURE : PLATFORM_SUCCESS;
}

int detachThread(HANDLE thrd) {
    return CloseHandle(thrd) ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
}

int waitChildren() {
    return PLATFORM_SUCCESS;
}

int daemonize() {
    return PLATFORM_SUCCESS;
}

int initSemaphore(semaphore_t* pSem, int initial, int max) {
    if (!pSem) return PLATFORM_FAILURE;
    return (*pSem = CreateSemaphore(NULL, initial, max, NULL)) ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
}

int waitSemaphore(semaphore_t* pSem, int timeout) {
    if (!pSem) return PLATFORM_FAILURE;
    if (timeout <= 0) timeout = INFINITE;
    return WaitForSingleObject(*pSem, timeout) != WAIT_FAILED ? PLATFORM_SUCCESS : PLATFORM_FAILURE;  // todo timeout
}

int sigSemaphore(semaphore_t* pSem) {
    if (!pSem) return PLATFORM_FAILURE;
    return ReleaseSemaphore(*pSem, 1, NULL);
}

int destroySemaphore(semaphore_t* pSem) {
    if (!pSem) return PLATFORM_FAILURE;
    return CloseHandle(*pSem) ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
}

/*********************************************** FILES  ***************************************************************/

bool isPathRelative(cstring_t path) {
    return path && path[0] != '\0' && path[1] != ':' && path[0] != '\\';
}

string_t getRealPath(cstring_t relative, string_t absolute, bool acceptAbsent) {
    int attr = fileAttributes(relative);
    if (!(PLATFORM_FAILURE & attr) || (attr & PLATFORM_NOT_FOUND && acceptAbsent)) {
        return _fullpath(absolute, relative, 0);
    }
    return NULL;
}

file_size_t getFileSize(const char* path) {
    HANDLE file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (INVALID_HANDLE_VALUE == file) {
        printf("ma come?? %d\n", GetLastError());
        return -1;
    }
    LARGE_INTEGER li;
    if (!GetFileSizeEx(file, &li)) {
        CloseHandle(file);
        return -1;
    }
    CloseHandle(file);
    return li.QuadPart;
}

int fileAttributes(LPCSTR path) {
    DWORD attr = GetFileAttributes(path);
    if (INVALID_FILE_ATTRIBUTES == attr) {
        return (GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND)
                   ? PLATFORM_FAILURE | PLATFORM_NOT_FOUND
                   : PLATFORM_FAILURE;
    }
    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        return PLATFORM_ISDIR;
    } else {
        return PLATFORM_ISFILE;
    }
}

int getFileMap(cstring_t path, file_mapping_t* mapData, file_size_t offset, size_t length) {
    HANDLE file = NULL;
    if (NULL == mapData->memMap) {
        if (INVALID_HANDLE_VALUE == (file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL))) {
            goto ON_ERROR;
        }
        LARGE_INTEGER fileSize;
        if (!GetFileSizeEx(file, &fileSize)) {
            goto ON_ERROR;
        }
        if (fileSize.QuadPart == 0) {
            mapData->view = NULL;
            mapData->size = 0;
            return PLATFORM_SUCCESS;
        } else {
            mapData->totalSize = fileSize.QuadPart;
        }
        OVERLAPPED ovlp;
        memset(&ovlp, 0, sizeof(ovlp));
        if (!LockFileEx(file, LOCKFILE_EXCLUSIVE_LOCK, 0, LO32(mapData->totalSize), HI32(mapData->totalSize), &ovlp)) {
            goto ON_ERROR;
        }
        if (NULL == (mapData->memMap = CreateFileMapping(file, NULL, PAGE_READONLY, 0, 0, NULL))) {
            UnlockFileEx(file, 0, LO32(mapData->totalSize), HI32(mapData->totalSize), &ovlp);
            goto ON_ERROR;
        }
        if (!UnlockFileEx(file, 0, LO32(mapData->totalSize), HI32(mapData->totalSize), &ovlp)) {
            goto ON_ERROR;
        }
        if (!CloseHandle(file)) {
        }
    }
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    offset -= offset % sysInfo.dwAllocationGranularity;
    if (length < mapData->totalSize - offset) {
        length -= length % sysInfo.dwAllocationGranularity;
    } else {
        length = mapData->totalSize - offset;
    }
    mapData->size = length;
    if (NULL == (mapData->view = MapViewOfFile(mapData->memMap, FILE_MAP_READ, HI32(offset), LO32(offset), length))) {
        goto ON_ERROR;
    }
    return PLATFORM_SUCCESS;
ON_ERROR:
    if (file) {
        CloseHandle(file);
    }
    return PLATFORM_FAILURE;
}

int iterateDir(LPCSTR path, HANDLE* dir, LPSTR name, size_t nameSize) {
    WIN32_FIND_DATA data;
    if (*dir == NULL) {
        char dirPath[MAX_NAME + 2];
        snprintf(dirPath, sizeof(dirPath), "%s/*", path);
        if ((*dir = FindFirstFile(dirPath, &data)) == INVALID_HANDLE_VALUE) {
            return (GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND)
                       ? PLATFORM_FAILURE | PLATFORM_NOT_FOUND
                       : PLATFORM_FAILURE;
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

int unmapMem(file_mapping_t* pMap, bool closeHandle) {
    if (!pMap) return PLATFORM_FAILURE;
    int ret = PLATFORM_SUCCESS;
    if (pMap->size > 0) {
        ret = UnmapViewOfFile(pMap->view) ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
    }
    if (closeHandle && pMap->memMap) {
        ret = CloseHandle(pMap->memMap) ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
    }
    return ret;
}

/*****************************************************************************************************************/
/*                                             LINUX FUNCTIONS                                                    */

/*****************************************************************************************************************/

#else

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

/************************************************** UTILS ********************************************************/

int getCwd(char *dst, size_t size) {
    return getcwd(dst, size) ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
}

int changeCwd(const char *path) {
    return chdir(path) >= 0 ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
}

void debugMessage(cstring_t message, int level) {
    if (debugLevel < level) {
        return;
    }
    int priority = 0;
    string_t lvl = "";
    switch (level) {
        case DBG_DEBUG:
            lvl = "DEBUG";
            priority = LOG_DEBUG;
            break;
        case DBG_INFO:
            lvl = "INFO";
            priority = LOG_INFO;
            break;
        case DBG_WARN:
            lvl = "WARNING";
            priority = LOG_WARNING;
            break;
        case DBG_ERR:
            lvl = "ERROR";
            priority = LOG_ERR;
    }
    syslog(priority, "%s - %s\n%s", lvl, message, (level == DBG_ERR) ? strerror(errno) : "");
}

int getWindowsHelpersPaths() {
    return PLATFORM_SUCCESS;
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

int startThread(pthread_t *thrd, LPTHREAD_START_ROUTINE routine, void *args) {
    return pthread_create(thrd, NULL, routine, args) == 0 ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
}

int detachThread(pthread_t thrd) {
    return pthread_detach(thrd) == 0 ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
}

int waitChildren() {
    int ret;
    while ((ret = waitpid(-1, NULL, WNOHANG)) > 0)
        ;
    if (ret < 0 && errno != ECHILD) {
        return PLATFORM_FAILURE;
    }
    return PLATFORM_SUCCESS;
}

int initSemaphore(semaphore_t *pSem, int initial, int max) {
    if (!pSem) return PLATFORM_FAILURE;
    return sem_init(pSem, initial, 0) == 0 ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
}

int waitSemaphore(semaphore_t *pSem, int timeout) {
    if (!pSem) return PLATFORM_FAILURE;
    if (timeout > 0) {
        struct timespec ts;
        if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
            return PLATFORM_FAILURE;
        }
        ts.tv_sec += (timeout / 1000);
        return sem_timedwait(pSem, &ts) == 0 ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
    } else {
        return sem_wait(pSem) == 0 ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
    }
}

int sigSemaphore(semaphore_t *pSem) {
    if (!pSem) return PLATFORM_FAILURE;
    return sem_post(pSem) == 0 ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
}

int destroySemaphore(semaphore_t *pSem) {
    if (!pSem) return PLATFORM_FAILURE;
    return sem_destroy(pSem) == 0 ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
}

int daemonize() {
    int pid;
    pid = fork();
    if (pid < 0) {
        return PLATFORM_FAILURE;
    } else if (pid > 0) {
        exit(0);
    } else {
        sigset_t set;
        if (setsid() < 0) {
            return PLATFORM_FAILURE;
        }
        if (sigemptyset(&set) < 0) {
            return PLATFORM_FAILURE;
        }
        if (sigaddset(&set, SIGHUP) < 0) {
            return PLATFORM_FAILURE;
        }
        if (sigprocmask(SIG_BLOCK, &set, NULL) < 0) {
            return PLATFORM_FAILURE;
        }
        pid = fork();
        if (pid > 0) {
            printf("Pid %d\n", pid);
            exit(0);
        } else if (pid < 0) {
            return PLATFORM_FAILURE;
        } else {
            umask(S_IRUSR & S_IWUSR & S_IRGRP & S_IWGRP);
            if (sigprocmask(SIG_UNBLOCK, &set, NULL) < 0) {
                return PLATFORM_FAILURE;
            }
            int devNull;
            if ((devNull = open("/dev/null", O_RDWR)) < 0) {
                return PLATFORM_FAILURE;
            }
            // if (dup2(devNull, STDIN_FILENO) < 0 || dup2(devNull, STDOUT_FILENO) < 0 || dup2(devNull, STDERR_FILENO) < 0) {
            //     return PLATFORM_FAILURE;
            // }
            return close(devNull) == 0 ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
        }
    }
}

/*********************************************** FILES ****************************************************************/

bool isPathRelative(cstring_t path) {
    return path[0] != '/';
}

string_t getRealPath(cstring_t relative, string_t absolute, bool acceptAbsent) {
    string_t ret = NULL;
    int attr = fileAttributes(relative);
    if (!(PLATFORM_FAILURE & attr)) {
        ret = realpath(relative, absolute);
    } else if (attr & PLATFORM_NOT_FOUND && acceptAbsent) {
        int fd = creat(relative, S_IRWXU);
        if (fd < 0) {
            return NULL;
        }
        close(fd);
        ret = realpath(relative, absolute);
        if (remove(relative) < 0) {
            if (ret) free(ret);
            return NULL;
        }
    }
    return ret;
}

int fileAttributes(cstring_t path) {
    struct stat statbuf;
    if (stat(path, &statbuf) != 0) {
        return errno == ENOENT ? PLATFORM_FAILURE | PLATFORM_NOT_FOUND : PLATFORM_FAILURE;
    }
    if (S_ISREG(statbuf.st_mode)) {
        return PLATFORM_ISFILE;
    } else if (S_ISDIR(statbuf.st_mode)) {
        return PLATFORM_ISDIR;
    } else {
        return PLATFORM_FAILURE;
    }
}

file_size_t getFileSize(cstring_t path) {
    struct stat statBuf;
    if (stat(path, &statBuf) < 0) {
        return -1;
    }
    return statBuf.st_size;
}

int getFileMap(cstring_t path, file_mapping_t *mapData, file_size_t offset, size_t length) {
    int fd = -1;
    struct flock lck;
    lck.l_type = F_RDLCK;
    lck.l_start = 0;
    lck.l_whence = SEEK_SET;
    lck.l_len = 0;
    lck.l_pid = getpid();
    if ((fd = open(path, O_RDONLY)) < 0) {
        goto ON_ERROR;
    }
    if (fcntl(fd, F_SETLKW, &lck) < 0) {
        goto ON_ERROR;
    }
    struct stat statBuf;
    if (fstat(fd, &statBuf) < 0) {
        goto ON_ERROR;
    }
    mapData->totalSize = statBuf.st_size;
    off_t pageSize = sysconf(_SC_PAGE_SIZE);
    offset -= offset % pageSize;
    // pa_offset = offset & ~(sysconf(_SC_PAGE_SIZE) - 1);
    if (length < mapData->totalSize - offset) {
        length -= length % pageSize;
    } else {
        length = mapData->totalSize - offset;
    }
    if (MAP_FAILED == (mapData->view = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, offset))) {
        goto ON_ERROR;
    }
    mapData->size = length;
    lck.l_type = F_UNLCK;
    if (fcntl(fd, F_SETLK, &lck) < 0 ||
        close(fd) < 0) {
        goto ON_ERROR;
    }
    return PLATFORM_SUCCESS;
ON_ERROR:
    if (fd > 0) close(fd);
    return PLATFORM_FAILURE;
}

int iterateDir(cstring_t path, DIR **dir, string_t name, size_t nameSize) {
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
    if (snprintf(name, nameSize, "%s%s%s", isCurrent ? "" : path, isCurrent ? "" : "/", entry->d_name) > nameSize) {
        return PLATFORM_FAILURE;
    }
    return PLATFORM_SUCCESS;
}

int closeDir(DIR *dir) {
    return closedir(dir) == 0 ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
}

int unmapMem(file_mapping_t *pMap, bool _ignored) {
    if (pMap->size > 0) {
        int ret = munmap(pMap->view, pMap->size) == 0 ? PLATFORM_SUCCESS : PLATFORM_FAILURE;
        pMap->view = NULL;
        return ret;
    }
    return PLATFORM_SUCCESS;
}

#endif