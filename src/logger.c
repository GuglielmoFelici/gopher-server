#include "../headers/logger.h"
#include <stdio.h>

#if defined(_WIN32)

/*****************************************************************************************************************/
/*                                             WINDOWS FUNCTIONS                                                 */

/*****************************************************************************************************************/

int logTransfer(const logger_t* pLogger, LPCSTR log) {
    DWORD written;
    if (!pLogger) {
        return LOGGER_FAILURE;
    }
    WaitForSingleObject(*(pLogger->pLogMutex), INFINITE);
    if (!WriteFile(pLogger->logPipe, log, strlen(log), &written, NULL)) {
        printf("log %d\n", GetLastError());
        return LOGGER_FAILURE;
    }
    if (!SetEvent(pLogger->logEvent)) {
        return LOGGER_FAILURE;
    }
    if (!ReleaseMutex(*(pLogger->pLogMutex))) {
        return LOGGER_FAILURE;
    };
    return LOGGER_SUCCESS;
}

/* Avvia il processo di logging dei trasferimenti. */
int startTransferLog(logger_t* pLogger) {
    char exec[MAX_NAME] = "";
    SECURITY_ATTRIBUTES attr;
    memset(&attr, 0, sizeof(attr));
    attr.bInheritHandle = TRUE;
    attr.nLength = sizeof(attr);
    attr.lpSecurityDescriptor = NULL;
    HANDLE readPipe = NULL;
    HANDLE writePipe = NULL;
    HANDLE* pLogMutex = NULL;
    if (!pLogger) {
        goto ON_ERROR;
    }
    snprintf(exec, sizeof(exec), "%s/" LOGGER_PATH, pLogger->installationDir);
    if (!CreatePipe(&readPipe, &writePipe, &attr, 0)) {
        goto ON_ERROR;
    }
    pLogger->logPipe = writePipe;
    if (NULL == (pLogMutex = malloc(sizeof(HANDLE)))) {
        goto ON_ERROR;
    }
    // Mutex per proteggere le scritture sulla pipe
    *pLogMutex = CreateMutex(&attr, FALSE, LOG_MUTEX_NAME);
    if (NULL == *pLogMutex) {
        goto ON_ERROR;
    }
    pLogger->pLogMutex = pLogMutex;
    // Evento per notificare al logger che ci sono dati da leggere sulla pipe
    HANDLE logEvent = CreateEvent(&attr, FALSE, FALSE, LOGGER_EVENT_NAME);
    if (NULL == logEvent) {
        goto ON_ERROR;
    }
    pLogger->logEvent = logEvent;
    STARTUPINFO startupInfo;
    PROCESS_INFORMATION processInfo;
    memset(&startupInfo, 0, sizeof(startupInfo));
    memset(&processInfo, 0, sizeof(processInfo));
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdInput = readPipe;
    startupInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startupInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    if (!CreateProcess(exec, NULL, NULL, NULL, TRUE, 0, NULL, pLogger->installationDir, &startupInfo, &processInfo)) {
        goto ON_ERROR;
    }
    pLogger->pid = processInfo.dwProcessId;
    CloseHandle(readPipe);
    return LOGGER_SUCCESS;
ON_ERROR:
    if (readPipe) {
        CloseHandle(readPipe);
    }
    if (writePipe) {
        CloseHandle(writePipe);
    }
    if (logEvent) {
        CloseHandle(logEvent);
    }
    if (pLogMutex) {
        free(pLogMutex);
    }
    return LOGGER_FAILURE;
}

int stopLogger(logger_t* pLogger) {
    if (!pLogger) {
        return LOGGER_FAILURE;
    }
    if (!TerminateProcess(OpenProcess(DELETE, FALSE, pLogger->pid), 0)) {
        return LOGGER_FAILURE;
    }
    pLogger->pid = -1;
    CloseHandle(*(pLogger->pLogMutex));
    if (pLogger->pLogMutex) {
        free(pLogger->pLogMutex);
    }
    if (pLogger->pLogCond) {
        free(pLogger->pLogCond);
    }
    if (!CloseHandle(pLogger->logEvent) ||
        !CloseHandle(pLogger->logPipe)) {
        return LOGGER_FAILURE;
    }
    return LOGGER_SUCCESS;
}

#else

/*****************************************************************************************************************/
/*                                             LINUX FUNCTIONS                                                    */

/*****************************************************************************************************************/

#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int stopLogger(logger_t* pLogger) {
    if (!pLogger) {
        return LOGGER_FAILURE;
    }
    if (pLogger->pid < 0 || kill(pLogger->pid, 0) != 0 || kill(pLogger->pid, SIGINT) != 0) {
        return LOGGER_FAILURE;
    }
    pLogger->pid = -1;
    if (close(pLogger->logPipe) != 0) {
        return LOGGER_FAILURE;
    }
    if (pLogger->pLogMutex) {
        if (munmap(pLogger->pLogMutex, sizeof(pthread_mutex_t)) != 0) {
            return LOGGER_FAILURE;
        }
    }
    if (pLogger->pLogCond) {
        if (munmap(pLogger->pLogCond, sizeof(pthread_cond_t)) != 0) {
            return LOGGER_FAILURE;
        }
    }
    return LOGGER_SUCCESS;
}

/* Effettua una scrittura sulla pipe di logging */
int logTransfer(const logger_t* pLogger, const char* log) {
    if (
        !pLogger ||
        pthread_mutex_lock(pLogger->pLogMutex) < 0 ||
        write(pLogger->logPipe, log, strlen(log)) < 0 ||
        pthread_cond_signal(pLogger->pLogCond) < 0 ||
        pthread_mutex_unlock(pLogger->pLogMutex) < 0) {
        return LOGGER_FAILURE;
    }
    return LOGGER_SUCCESS;
}

/* Loop di logging */
static void loggerLoop(const logger_t* pLogger) {
    int logFile = -1;
    char buff[MAX_LINE_SIZE];
    char logFilePath[MAX_NAME];
    if (!pLogger) {
        goto ON_ERROR;
    }
    if (pthread_mutex_lock(pLogger->pLogMutex) < 0) {
        goto ON_ERROR;
    }
    printf("Logger mutex locked\n");
    prctl(PR_SET_NAME, "Gopher logger");
    prctl(PR_SET_PDEATHSIG, SIGINT);
    if (snprintf(logFilePath, sizeof(logFilePath), "%s/logFile", pLogger->installationDir) >= sizeof(logFilePath)) {
        printf("nome logger troppo lungo");
        goto ON_ERROR;
    }
    // i diritti sono giusti?
    if ((logFile = creat(logFilePath, S_IRWXU)) < 0) {
        printf("Can't start logger\n");
        goto ON_ERROR;
    }
    while (1) {
        size_t bytesRead;
        pthread_cond_wait(pLogger->pLogCond, pLogger->pLogMutex);
        printf("Logger entered cond\n");
        if ((bytesRead = read(pLogger->logPipe, buff, sizeof(buff))) < 0) {
            printf("Logger err\n");
            goto ON_ERROR;
        } else {
            if (write(logFile, buff, bytesRead) < 0) {
                printf("Failed logging\n");
            }
        }
        printf("Logger end of loop\n");
    }
ON_ERROR:
    if (logFile >= 0) {
        close(logFile);
    }
    exit(1);
}

/* Avvia il processo di logging dei trasferimenti. */
int startTransferLog(logger_t* pLogger) {
    int pid;
    int pipeFd[2] = {-1, -1};
    pthread_mutex_t mutex;
    pthread_mutexattr_t mutexAttr;
    pthread_cond_t cond;
    pthread_condattr_t condAttr;
    pthread_mutex_t* pMutex = MAP_FAILED;
    pthread_cond_t* pCond = MAP_FAILED;
    if (!pLogger) {
        goto ON_ERROR;
    }
    // Inizializza mutex e condition variable per notificare il logger che sono pronti nuovi dati sulla pipe.
    // TODO controlla cascata
    if (pthread_mutexattr_init(&mutexAttr) < 0 ||
        pthread_mutexattr_setpshared(&mutexAttr, PTHREAD_PROCESS_SHARED) < 0 ||
        pthread_mutex_init(&mutex, &mutexAttr) < 0 ||
        pthread_condattr_init(&condAttr) < 0 ||
        pthread_condattr_setpshared(&condAttr, PTHREAD_PROCESS_SHARED) < 0 ||
        pthread_cond_init(&cond, &condAttr) < 0) {
        goto ON_ERROR;
    }
    pMutex = mmap(NULL, sizeof(pthread_mutex_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (MAP_FAILED == pMutex) {
        goto ON_ERROR;
    }
    *pMutex = mutex;
    pCond = mmap(NULL, sizeof(pthread_cond_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (MAP_FAILED == pCond) {
        goto ON_ERROR;
    }
    *pCond = cond;
    if (pipe(pipeFd) < 0) {
        goto ON_ERROR;
    }
    if ((pid = fork()) < 0) {
        goto ON_ERROR;
    } else if (pid == 0) {  // Logger
        struct sigaction act;
        act.sa_handler = SIG_DFL;
        if (sigaction(SIGINT, &act, NULL) < 0) {
            goto ON_ERROR;
        }
        close(pipeFd[1]);
        pLogger->pLogCond = pCond;
        pLogger->pLogMutex = pMutex;
        pLogger->pid = getpid();
        pLogger->logPipe = pipeFd[0];
        loggerLoop(pLogger);
    } else {  // Server
        close(pipeFd[0]);
        pLogger->pLogCond = pCond;
        pLogger->pLogMutex = pMutex;
        pLogger->logPipe = pipeFd[1];
        pLogger->pid = pid;
        return LOGGER_SUCCESS;
    }
ON_ERROR:
    if (MAP_FAILED != pMutex) {
        munmap(pLogger, sizeof(pthread_mutex_t));
    }
    if (MAP_FAILED != pCond) {
        munmap(pLogger, sizeof(pthread_cond_t));
    }
    if (pipeFd[0] == -1) {
        close(pipeFd[0]);
    }
    if (pipeFd[1] == -1) {
        close(pipeFd[1]);
    }
    return LOGGER_FAILURE;
}

#endif

/*****************************************************************************************************************/
/*                                             COMMON FUNCTIONS                                                 */

/*****************************************************************************************************************/

int initLogger(logger_t* pLogger) {
    if (!pLogger) {
        return LOGGER_FAILURE;
    }
    pLogger->pid = -1;
    pLogger->pLogCond = NULL;
    pLogger->pLogMutex = NULL;
    strncpy(pLogger->installationDir, "", sizeof(pLogger->installationDir));
    return LOGGER_SUCCESS;
}