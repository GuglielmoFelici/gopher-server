#ifndef LOGGER_H
#define LOGGER_H

#include "datatypes.h"

#define LOGGER_PATH "src\\helpers\\winLogger.exe"
#define LOG_MUTEX_NAME "logMutex"
#define LOGGER_EVENT_NAME "logEvent"
// Error codes
#define LOGGER_SUCCESS 0
#define LOGGER_FAILURE -1

typedef struct {
    pipe_t logPipe;      // Pipe per il log
    mutex_t* pLogMutex;  // Puntatore al mutex per proteggere la pipe di logging
    cond_t* pLogCond;    // Puntatore alla condition variable per il risveglio del logger su Linux
    event_t logEvent;    // Evento per lo il risveglio del logger su windows
    proc_id_t pid;
    char installationDir[MAX_PATH];
} logger_t;

typedef int xanax_t;

int startTransferLog(logger_t* logger);

int logTransfer(logger_t* pLogger, LPSTR log);

#endif