/** \file logger.h
 * Functions to handle the logging of the gopher file transfers.
 * @file logger.c
 */

#ifndef LOGGER_H
#define LOGGER_H

#include "datatypes.h"

#define LOGGER_PATH "src\\helpers\\winLogger.exe"
#define LOG_FILE "logFile"
#define LOG_MUTEX_NAME "logMutex"
#define LOGGER_EVENT_NAME "logEvent"
#define MAX_LINE_SIZE 100
#define LOGGER_SUCCESS 0
#define LOGGER_FAILURE -1

/**
 * A struct representing an instance of a transfer log
*/
typedef struct {
    /** Pipe to use for read/write */
    pipe_t logPipe;
    /** Pointer to the mutex guarding the log pipe */
    mutex_t* pLogMutex;
    /** [Linux only] Pointer to the condition variable to notify the logger for incoming data */
    cond_t* pLogCond;
    /** [Windows only] Event to notify the logger for incoming data */
    event_t logEvent;
    /** The pid of the log process */
    proc_id_t pid;
    /** The base installation directory of the logger */
    char installationDir[MAX_NAME];
} logger_t;

/**
 * Initializes a logger_t struct for further usage. 
 * @param pLogger A pointer to the struct to initialize.
 * @return LOGGER_SUCCESS or LOGGER_FAILURE.
 * @see logger_t
 */
int initLogger(logger_t* pLogger);

/**
 * Stops a logging process.
 * @param pLogger A pointer to the struct representing the logger process to stop.
 * @return LOGGER_SUCCESS or LOGGER_FAILURE.
 * @see logger_t
 */
int stopLogger(logger_t* pLogger);

/**
 * Starts a logging process represented by a logger_t struct.
 * @param pLogger The struct containing the information needed to start the process.
 * @return LOGGER_SUCCESS or LOGGER_FAILURE.
 * @see initLogger()
 * @see logger_t
 */
int startTransferLog(logger_t* pLogger);

/**
 * Passes a message to a logging process.
 * @param pLogger The struct representing the logging process.
 * @param log The message to log.
 * @return LOGGER_SUCCESS or LOGGER_FAILURE.
 * @see logger_t
 */
int logTransfer(const logger_t* pLogger, cstring_t log);

#endif