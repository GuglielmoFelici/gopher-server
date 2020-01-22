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

/*********************************************** LOGGER ***************************************************************/

int logTransfer(LPSTR log) {
    // TODO mutex
    DWORD written;
    WaitForSingleObject(*logMutex, INFINITE);
    if (!WriteFile(logPipe, log, strlen(log), &written, NULL)) {
        return GOPHER_FAILURE;
    }
    if (!SetEvent(logEvent)) {
        return GOPHER_FAILURE;
    }
    if (!ReleaseMutex(logMutex)) {
        return GOPHER_FAILURE;
    };
    return GOPHER_SUCCESS;
}

/* Avvia il processo di logging dei trasferimenti. */
int startTransferLog() {
    char exec[MAX_NAME];
    HANDLE readPipe = NULL;
    SECURITY_ATTRIBUTES attr;
    STARTUPINFO startupInfo;
    PROCESS_INFORMATION processInfo;
    snprintf(exec, sizeof(exec), "%s/" LOGGER_PATH, installationDir);
    memset(&attr, 0, sizeof(attr));
    attr.bInheritHandle = TRUE;
    attr.nLength = sizeof(attr);
    attr.lpSecurityDescriptor = NULL;
    // logPipe è globale/condivisa, viene acceduta in scrittura quando avviene un trasferimento file
    if (!CreatePipe(&readPipe, &logPipe, &attr, 0)) {
        goto ON_ERROR;
    }
    if ((logMutex = malloc(sizeof(HANDLE))) == NULL) {
        goto ON_ERROR;
    }
    // Mutex per proteggere le scritture sulla pipe
    if ((*logMutex = CreateMutex(&attr, FALSE, LOG_MUTEX_NAME)) == NULL) {
        goto ON_ERROR;
    }
    // Evento per notificare al logger che ci sono dati da leggere sulla pipe
    if ((logEvent = CreateEvent(&attr, FALSE, FALSE, LOGGER_EVENT_NAME)) == NULL) {
        goto ON_ERROR;
    }
    memset(&startupInfo, 0, sizeof(startupInfo));
    memset(&processInfo, 0, sizeof(processInfo));
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdInput = readPipe;
    startupInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startupInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    if (!CreateProcess(exec, NULL, NULL, NULL, TRUE, 0, NULL, installationDir, &startupInfo, &processInfo)) {
        goto ON_ERROR;
    }
    loggerPid = processInfo.dwProcessId;
    CloseHandle(readPipe);
ON_ERROR:
    if (logPipe) {
        CloseHandle(logPipe);
    }
    if (logEvent) {
        CloseHandle(logEvent);
    }
    if (readPipe) {
        CloseHandle(readPipe);
    }
    if (logMutex) {
        free(logMutex);
    }
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

bool loggerShutdown = false;

/* Effettua una scrittura sulla pipe di logging */
int logTransfer(char *log) {
    if (
        pthread_mutex_lock(logMutex) < 0 ||
        write(logPipe, log, strlen(log)) < 0 ||
        pthread_cond_signal(logCond) < 0 ||
        pthread_mutex_unlock(logMutex) < 0) {
        return GOPHER_FAILURE;
    }
    return GOPHER_SUCCESS;

    return pthread_mutex_lock(logMutex) > 0 &&
           write(logPipe, log, strlen(log)) > 0 &&
           pthread_cond_signal(logCond) > 0 &&
           pthread_mutex_unlock(logMutex) > 0;
}

/* Loop di logging */
void loggerLoop(int inPipe) {
    int logFile, exitCode = GOPHER_SUCCESS;
    char buff[PIPE_BUF];
    char logFilePath[MAX_NAME];
    if (pthread_mutex_lock(logMutex) < 0) {
        exitCode = GOPHER_FAILURE;
        goto ON_EXIT;
    }
    printf("Logger mutex locked\n");
    prctl(PR_SET_NAME, "Gopher logger");
    prctl(PR_SET_PDEATHSIG, SIGINT);
    snprintf(logFilePath, sizeof(logFilePath), "%s/logFile", installationDir);
    if ((logFile = open(logFilePath, O_WRONLY | O_CREAT | O_APPEND, S_IRWXU)) < 0) {
        printf(WARN " - Can't start logger\n");
        goto ON_EXIT;
    }
    while (1) {
        size_t bytesRead;
        if (loggerShutdown) {
            printf("logger requested shutdown\n");
            goto ON_EXIT;
        }
        pthread_cond_wait(logCond, logMutex);
        printf("Logger entered cond\n");
        if ((bytesRead = read(inPipe, buff, PIPE_BUF)) < 0) {
            printf("Logger err\n");
            exitCode = GOPHER_FAILURE;
            goto ON_EXIT;
        } else {
            if (strcmp(buff, "EXIT") == 0) {
                goto ON_EXIT;
            }
            if (write(logFile, buff, bytesRead) < 0) {
                printf(WARN " - Failed logging\n");
            }
        }
        printf("Logger end of loop\n");
    }
ON_EXIT:
    pthread_mutex_unlock(logMutex);
    munmap(logMutex, sizeof(pthread_mutex_t));
    munmap(logCond, sizeof(pthread_cond_t));
    close(inPipe);
    close(logFile);
    exit(exitCode);
}

/* Avvia il processo di logging dei trasferimenti. */
void startTransferLog() {
    int pid;
    int pipeFd[2];
    pthread_mutex_t mutex;
    pthread_mutexattr_t mutexAttr;
    pthread_cond_t cond;
    pthread_condattr_t condAttr;
    // Inizializza mutex e condition variable per notificare il logger che sono pronti nuovi dati sulla pipe.
    if (pthread_mutexattr_init(&mutexAttr) < 0 ||
        pthread_mutexattr_setpshared(&mutexAttr, PTHREAD_PROCESS_SHARED) < 0 ||
        pthread_mutex_init(&mutex, &mutexAttr) < 0 ||
        pthread_condattr_init(&condAttr) < 0 ||
        pthread_condattr_setpshared(&condAttr, PTHREAD_PROCESS_SHARED) < 0 ||
        pthread_cond_init(&cond, &condAttr) < 0) {
        _err("startTransferLog() - impossibile inizializzare i mutex\n", true, -1);
    }
    logMutex = mmap(NULL, sizeof(pthread_mutex_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (logMutex == MAP_FAILED) {
        _err("startTransferLog() - impossibile mappare il mutex in memoria\n", true, -1);
    }
    *logMutex = mutex;
    logCond = mmap(NULL, sizeof(pthread_cond_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (logCond == MAP_FAILED) {
        _err("startTransferLog() - impossibile mappare la condition variable in memoria\n", true, -1);
    }
    *logCond = cond;
    if (pipe(pipeFd) < 0) {
        _err("startTransferLog() - impossibile aprire la pipe\n", true, -1);
    }
    if ((pid = fork()) < 0) {
        _err("startTransferLog() - fork fallita\n", true, pid);
    } else if (pid == 0) {  // Logger
        loggerPid = getpid();
        close(pipeFd[1]);
        loggerLoop(pipeFd[0]);
    } else {  // Server
        close(pipeFd[0]);
        logPipe = pipeFd[1];  // logPipe è globale, viene acceduta per inviare i dati al logger
        loggerPid = pid;
    }
}

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
