#include "../headers/logger.h"
#include <stdio.h>

#if defined(_WIN32)

/*****************************************************************************************************************/
/*                                             WINDOWS FUNCTIONS                                                 */

/*****************************************************************************************************************/

int logTransfer(logger_t* pLogger, LPSTR log) {
    // TODO mutex
    DWORD written;
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

/* Avvia il processo di logging dei trasferimenti. */
int startTransferLog(logger_t* pLogger) {
    char exec[MAX_NAME];
    HANDLE readPipe = NULL;
    SECURITY_ATTRIBUTES attr;
    STARTUPINFO startupInfo;
    PROCESS_INFORMATION processInfo;
    snprintf(exec, sizeof(exec), "%s/" LOGGER_PATH, pLogger->installationDir);
    memset(&attr, 0, sizeof(attr));
    attr.bInheritHandle = TRUE;
    attr.nLength = sizeof(attr);
    attr.lpSecurityDescriptor = NULL;
    // logPipe è globale/condivisa, viene acceduta in scrittura quando avviene un trasferimento file
    HANDLE logPipe = NULL;
    if (!CreatePipe(&readPipe, &logPipe, &attr, 0)) {
        goto ON_ERROR;
    }
    pLogger->logPipe = logPipe;
    if (NULL == (pLogger->pLogMutex = malloc(sizeof(HANDLE)))) {
        goto ON_ERROR;
    }
    // Mutex per proteggere le scritture sulla pipe
    HANDLE logMutex = CreateMutex(&attr, FALSE, LOG_MUTEX_NAME);
    if (NULL == logMutex) {
        goto ON_ERROR;
    }
    *(pLogger->pLogMutex) = logMutex;
    // Evento per notificare al logger che ci sono dati da leggere sulla pipe
    HANDLE logEvent = CreateEvent(&attr, FALSE, FALSE, LOGGER_EVENT_NAME);
    if (NULL == logEvent) {
        goto ON_ERROR;
    }
    pLogger->logEvent = logEvent;
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
    return LOGGER_FAILURE;
}

#else

/*****************************************************************************************************************/
/*                                             LINUX FUNCTIONS                                                    */

/*****************************************************************************************************************/

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