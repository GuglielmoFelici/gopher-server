#include "../headers/logger.h"

#include <stdio.h>
#include <string.h>

#include "../headers/globals.h"

#define MAX_LINE_SIZE 200

/** Path to the logfile */
string_t logPath = NULL;
/** Path to the windows logger executable*/
string_t winLoggerPath = NULL;

/** [Linux] Starts the main logging process loop.
 *  If pLogger is NULL or a system error occurs, the function logs and terminates the process with exit code 1
 *  @param pLogger A pointer to the logger_t struct representing a logging process.
*/
static void loggerLoop(const logger_t* pLogger);

/*****************************************************************************************************************/
/*                                             WINDOWS FUNCTIONS                                                 */

/*****************************************************************************************************************/

#if defined(_WIN32)

int startTransferLog(logger_t* pLogger) {
    HANDLE readPipe = NULL;
    HANDLE writePipe = NULL;
    HANDLE* pLogMutex = NULL;
    if (!pLogger) {
        goto ON_ERROR;
    }
    pLogger->pid = -1;
    char cmdLine[MAX_NAME];
    int bytesWritten = snprintf(cmdLine, sizeof(cmdLine), "%s %s", winLoggerPath, logPath);
    if (bytesWritten < 0 || bytesWritten >= sizeof(cmdLine)) {
        goto ON_ERROR;
    }
    SECURITY_ATTRIBUTES attr;
    memset(&attr, 0, sizeof(attr));
    attr.bInheritHandle = TRUE;
    attr.nLength = sizeof(attr);
    attr.lpSecurityDescriptor = NULL;
    if (!CreatePipe(&readPipe, &writePipe, &attr, 0)) {
        goto ON_ERROR;
    }
    if (NULL == (pLogMutex = malloc(sizeof(HANDLE)))) {
        goto ON_ERROR;
    }
    *pLogMutex = CreateMutex(&attr, FALSE, LOG_MUTEX_NAME);
    if (NULL == *pLogMutex) {
        goto ON_ERROR;
    }
    HANDLE logEvent = CreateEvent(&attr, FALSE, FALSE, LOGGER_EVENT_NAME);
    if (NULL == logEvent) {
        goto ON_ERROR;
    }
    pLogger->pLogMutex = pLogMutex;
    pLogger->logPipe = writePipe;
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
    if (!CreateProcess(winLoggerPath, cmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &startupInfo, &processInfo)) {
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
    if (pLogMutex) free(pLogMutex);
    return LOGGER_FAILURE;
}

int logTransfer(const logger_t* pLogger, LPCSTR log) {
    DWORD written;
    if (!pLogger) {
        return LOGGER_FAILURE;
    }
    WaitForSingleObject(*(pLogger->pLogMutex), INFINITE);
    if (!WriteFile(pLogger->logPipe, log, strlen(log), &written, NULL)) {
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

int stopTransferLog(logger_t* pLogger) {
    if (!pLogger) {
        return LOGGER_FAILURE;
    }
    if (!TerminateProcess(OpenProcess(DELETE, FALSE, pLogger->pid), 0)) {
        return LOGGER_FAILURE;
    }
    pLogger->pid = -1;
    CloseHandle(*(pLogger->pLogMutex));
    if (pLogger->pLogMutex) free(pLogger->pLogMutex);
    if (pLogger->pLogCond) free(pLogger->pLogCond);
    if (!CloseHandle(pLogger->logEvent) ||
        !CloseHandle(pLogger->logPipe)) {
        return LOGGER_FAILURE;
    }
    return LOGGER_SUCCESS;
}

/*****************************************************************************************************************/
/*                                             LINUX FUNCTIONS                                                    */

/*****************************************************************************************************************/

#else

#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#include "../headers/log.h"
#include "../headers/platform.h"

#define LOG_PROCESS_NAME "gopher-logger"

int startTransferLog(logger_t* pLogger) {
    if (!pLogger) {
        return LOGGER_FAILURE;
    }
    pLogger->pid = -1;
    int pipeFd[2] = {-1, -1};
    pthread_mutex_t* pMutex = NULL;
    pthread_cond_t* pCond = NULL;
    pthread_mutex_t mutex;
    pthread_mutexattr_t mutexAttr;
    pthread_cond_t cond;
    pthread_condattr_t condAttr;
    pMutex = mmap(NULL, sizeof(pthread_mutex_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (MAP_FAILED == pMutex) {
        goto ON_ERROR;
    }
    pCond = mmap(NULL, sizeof(pthread_cond_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (MAP_FAILED == pCond) {
        goto ON_ERROR;
    }
    // Inizializza mutex e condition variable per notificare il logger che sono pronti nuovi dati sulla pipe.
    if (
        pthread_mutexattr_init(&mutexAttr) != 0 ||
        pthread_mutexattr_setpshared(&mutexAttr, PTHREAD_PROCESS_SHARED) != 0 ||
        pthread_mutex_init(&mutex, &mutexAttr) != 0) {
        debugMessage(MUTEX_INIT_ERR, DBG_ERR);
        goto ON_ERROR;
    }
    if (
        pthread_condattr_init(&condAttr) != 0 ||
        pthread_condattr_setpshared(&condAttr, PTHREAD_PROCESS_SHARED) != 0 ||
        pthread_cond_init(&cond, &condAttr) != 0) {
        debugMessage(COND_INIT_ERR, DBG_ERR);
        goto ON_ERROR;
    }
    *pMutex = mutex;
    *pCond = cond;
    pLogger->pLogCond = pCond;
    pLogger->pLogMutex = pMutex;
    if (pipe(pipeFd) < 0) {
        goto ON_ERROR;
    }
    sigset_t set;
    if (sigemptyset(&set) < 0) {
        return LOGGER_FAILURE;
    }
    if (sigaddset(&set, SIGHUP) < 0) {
        return LOGGER_FAILURE;
    }
    if (sigprocmask(SIG_BLOCK, &set, NULL) < 0) {
        return LOGGER_FAILURE;
    }
    int pid;
    if ((pid = fork()) < 0) {
        debugMessage(FORK_FAILED, DBG_ERR);
        goto ON_ERROR;
    } else if (pid == 0) {  // Logger
        struct sigaction act;
        memset(&act, 0, sizeof(struct sigaction));
        act.sa_handler = SIG_DFL;
        if (sigaction(SIGINT, &act, NULL) < 0) {
            goto ON_ERROR;
        }
        if (close(pipeFd[1]) < 0) {
            debugMessage(PIPE_CLOSE_ERR, DBG_WARN);
        }
        pLogger->pid = getpid();
        pLogger->logPipe = pipeFd[0];
        loggerLoop(pLogger);
    } else {  // Server
        sigprocmask(SIG_UNBLOCK, &set, NULL);
        if (close(pipeFd[0]) < 0) {
            debugMessage(PIPE_CLOSE_ERR, DBG_WARN);
        }
        pLogger->logPipe = pipeFd[1];
        pLogger->pid = pid;
        return LOGGER_SUCCESS;
    }
ON_ERROR:
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);
    if (pMutex && MAP_FAILED != pMutex) {
        munmap(pLogger, sizeof(pthread_mutex_t));
    }
    if (pCond && MAP_FAILED != pCond) {
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

int logTransfer(const logger_t* pLogger, const char* log) {
    if (!pLogger) {
        return LOGGER_FAILURE;
    }
    if (!pthread_mutex_lock(pLogger->pLogMutex) < 0) {
        debugMessage(MUTEX_LOCK_ERR, DBG_ERR);
        return LOGGER_FAILURE;
    }
    if (write(pLogger->logPipe, log, strlen(log)) < 0) {
        debugMessage(LOGFILE_WRITE_ERR, DBG_ERR);
        return LOGGER_FAILURE;
    }
    if (pthread_cond_signal(pLogger->pLogCond) < 0) {
        debugMessage(COND_SIGNAL_ERR, DBG_ERR);
        return LOGGER_FAILURE;
    }
    if (pthread_mutex_unlock(pLogger->pLogMutex) < 0) {
        debugMessage(MUTEX_UNLOCK_ERR, DBG_ERR);
        return LOGGER_FAILURE;
    }
    return LOGGER_SUCCESS;
}

int stopTransferLog(logger_t* pLogger) {
    if (!pLogger) {
        return LOGGER_FAILURE;
    }
    if (pLogger->pid > 0 && kill(pLogger->pid, 0) == 0) {
        if (kill(pLogger->pid, SIGINT) == 0) {
            pLogger->pid = -1;
        } else {
            return LOGGER_FAILURE;
        }
    }
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

static void loggerLoop(const logger_t* pLogger) {
    int logFile = -1;
    char buff[MAX_LINE_SIZE];
    if (!pLogger) {
        goto ON_ERROR;
    }
    if (!logPath) {
        goto ON_ERROR;
    }
    if ((logFile = open(logPath, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRGRP | S_IROTH)) < 0) {
        debugMessage(LOGFILE_OPEN_ERR, DBG_ERR);
        goto ON_ERROR;
    }
    free(logPath);
    if (pthread_mutex_lock(pLogger->pLogMutex) != 0) {
        debugMessage(MUTEX_LOCK_ERR, DBG_ERR);
        goto ON_ERROR;
    }
    while (1) {
        size_t bytesRead;
        if (pthread_cond_wait(pLogger->pLogCond, pLogger->pLogMutex) != 0) {
            debugMessage(COND_WAIT_ERR, DBG_ERR);
            goto ON_ERROR;
        }
        if ((bytesRead = read(pLogger->logPipe, buff, sizeof(buff) - 3)) < 0) {  // - 3 is for the "..." for long lines
            debugMessage(PIPE_READ_ERR, DBG_ERR);
            goto ON_ERROR;
        } else {
            struct flock lck;
            lck.l_type = F_WRLCK;
            lck.l_start = 0;
            lck.l_whence = SEEK_SET;
            lck.l_len = 0;
            lck.l_pid = getpid();
            if (fcntl(logFile, F_SETLKW, &lck) < 0) {
                debugMessage(FILE_LOCK_ERR, DBG_ERR);
                goto ON_ERROR;
            }
            if (write(logFile, buff, bytesRead) < 0) {
                debugMessage(LOGFILE_WRITE_ERR, DBG_ERR);
                goto ON_ERROR;
            }
            if (bytesRead >= sizeof(buff) - 3) {
                if (write(logFile, "...", 3) < 0) {
                    debugMessage(LOGFILE_WRITE_ERR, DBG_ERR);
                    goto ON_ERROR;
                }
            }
            lck.l_type = F_UNLCK;
            if (fcntl(logFile, F_SETLK, &lck) < 0) {
                debugMessage(FILE_UNLOCK_ERR, DBG_ERR);
                goto ON_ERROR;
            }
        }
    }
ON_ERROR:
    if (logFile > 0) {
        close(logFile);
    }
    exit(1);
}

#endif