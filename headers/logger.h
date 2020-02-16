#ifndef LOGGER_H
#define LOGGER_H

#include "datatypes.h"

#define LOGGER_PATH "src\\helpers\\winLogger.exe"
#define LOG_FILE "logFile"
#define LOG_MUTEX_NAME "logMutex"
#define LOGGER_EVENT_NAME "logEvent"
#define MAX_LINE_SIZE 100
// Error codes
#define LOGGER_SUCCESS 0
#define LOGGER_FAILURE -1

typedef struct {
    pipe_t logPipe;      // Pipe per il log
    mutex_t* pLogMutex;  // Puntatore al mutex per proteggere la pipe di logging
    cond_t* pLogCond;    // Puntatore alla condition variable per il risveglio del logger su Linux
    event_t logEvent;    // Evento per il risveglio del logger su windows
    proc_id_t pid;
    char installationDir[MAX_NAME];
} logger_t;

int initLogger(logger_t* pLogger);

int stopLogger(logger_t* pLogger);

int startTransferLog(logger_t* pLogger);

int logTransfer(const logger_t* pLogger, cstring_t log);

#endif