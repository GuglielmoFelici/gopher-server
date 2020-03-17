/** \file logger.h
 * Functions to handle the logging of the gopher file transfers.
 * @file logger.c
 */

#ifndef LOGGER_H
#define LOGGER_H

#include "datatypes.h"
#include "globals.h"

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
} logger_t;

/**
 * Starts a logging process and writes its information in a logger_t struct.
 * @param pLogger The struct that will contain information about the logger.
 * @return LOGGER_SUCCESS if the process was started correctly,
 * LOGGER_FAILURE if pLogger is NULL or a system error occurs.
 * @see logger_t
 */
int startTransferLog(logger_t* pLogger);

/**
 * Stops a logging process.
 * NOTE: Be sure to initialize the logger_t struct by calling startTransferLog().
 * @param pLogger A pointer to the struct representing the logger process to stop.
 * @return LOGGER_SUCCESS or LOGGER_FAILURE.
 * @see logger_t
 */
int stopTransferLog(logger_t* pLogger);

/**
 * Passes a message to a logging process.
 * @param pLogger The struct representing the logging process.
 * @param log The message to log.
 * @return LOGGER_SUCCESS or LOGGER_FAILURE.
 * @see logger_t
 */
int logTransfer(const logger_t* pLogger, cstring_t log);

#endif