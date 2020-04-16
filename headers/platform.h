/** \file platform.h
 * Portable utility functions.
 * @file platform.c
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#if defined(_WIN32)
#include <windows.h>

#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <syslog.h>
#define HANDLE void*
#endif

#include <stdbool.h>
#include <stdlib.h>

#include "datatypes.h"

#define PLATFORM_SUCCESS 0x0001
#define PLATFORM_FAILURE 0x0002
#define PLATFORM_ISFILE 0x0004
#define PLATFORM_ISDIR 0x0008
#define PLATFORM_NOT_FOUND 0x0010
#define PLATFORM_END_OF_DIR 0x0020

/************************************************** UTILS ********************************************************/

/** @return true if the string str2 ends with the string str2. */
bool endsWith(cstring_t str1, cstring_t str2);

/** Copies the current working directory in dst.
 * @return PLATFORM_SUCCESS or PLATFORM_FAILURE.
*/
int getCwd(string_t dst, size_t size);

/** Changes the current working directory to path.
 * @return PLATFORM_SUCCESS or PLATFORM_FAILURE.
*/
int changeCwd(cstring_t path);

/** Logs a message to the current logger (syslog or stderr)
 * @param message The message to be logged.
 * @param level The verbosity level.
 * @see globals.h
*/
void debugMessage(cstring_t message, int level);

/**
 * Gets the absolute paths of windows helper files and stores them in the global variables winLogPath and winHelperPath.
 * @return PLATFORM_SUCCESS or PLATFORM_FAILURE.
*/
int getWindowsHelpersPaths();

/********************************************** SOCKETS *************************************************************/

/** @return the current system error relative to the sockets */
int sockErr();

/** Closes the socket s.
 * @return PLATFORM_SUCCESS or PLATFORM_FAILURE.
*/
int closeSocket(socket_t s);

/** Tries to send up to length bytes of data to the socket s.
 * @return PLATFORM_SUCCESS or PLATFORM_FAILURE.
*/
int sendAll(socket_t s, const void* data, file_size_t length);

/** @see inet_ntoa [Windows]
 *  @see inet_ntop [Linux]
*/
cstring_t inetNtoa(const struct in_addr* addr, void* dst, size_t size);

/*********************************************** THREADS & PROCESSES ***************************************************************/

/** Starts a new thread.
 * @param tid A pointer to a location where the thread id of the spawned thread will be written.
 * @param routine A pointer to the function to be executed by the new thread.
 * @param args A pointer to data used as arguments.
 * @return PLATFORM_SUCCESS or PLATFORM_FAILURE.
*/
int startThread(thread_t* tid, LPTHREAD_START_ROUTINE routine, void* args);

/** Detaches the thread tid. 
 * @return PLATFORM_SUCCESS or PLATFORM_FAILURE.
*/
int detachThread(thread_t tid);

/** [Linux] Tries to wait for all the children to finish, but doesn't block.
 * @return PLATFORM_SUCCESS or PLATFORM_FAILURE.
*/
int waitChildren();

/** [Linux] Transforms the current process into a daemon process. 
 * @return PLATFORM_SUCCESS or PLATFORM_FAILURE.
*/
int daemonize();

//TODO docs
int initSemaphore(semaphore_t* pSem, int initial, int max);

// TODO docs
int waitSemaphore(semaphore_t* pSem, int timeout);

// TODO docs
int sigSemaphore(semaphore_t* pSem);

int destroySemaphore(semaphore_t* pSem);

/*********************************************** FILES  ***************************************************************/

/** @return true if path is a relative path. */
bool isPathRelative(cstring_t path);

/**
 * Gets the absolute path of a file.
 * @param relative The relative path of the file to be converted to absolute.
 * @param absolute A pointer to a buffer where the absolute path will be stored.
 * @param acceptAbsent If false, the function fails if the relative path does not exist.
 * @return If absolute is NULL, the function returns a pointer to a heap-allocated string containing the path. Absolute is returned otherwise.
*/
string_t getRealPath(cstring_t relative, string_t absolute, bool acceptAbsent);

/** Retrieves the attributes of a file.
 * @return PLATFORM_ISFILE or PLATFORM_ISDIR. Upon error returns PLATFORM_FAILURE, or-ed with PLATFORM_NOT_FOUND if the file was not found.
*/
int fileAttributes(cstring_t path);

/** @return The size of the file path, or -1 upon failure.
*/
file_size_t getFileSize(cstring_t path);

/** Reads the next file of a directory and copies its relative path in name.
 *  If dir is NULL, opens the directory path and initializes dir for further calls.
 *  If dir is non-NULL, path is ignored (thus can be NULL), and the scan continues from the previouse iterateDir(dir) call.
 *  NOTE: dir must be closed using closeDir() when done.
 *  @param path The path to a directory to open. Ignored if dir is non-NULL.
 *  @param dir An object representing a directory. Used after the first call to iterateDir() for subsequent calls.
 *  @param name A buffer where the file relative path is to be copied.
 *  @param nameSize The size of the buffer name.
 *  @return PLATFORM_SUCCESS or PLATFORM_FAILURE. PLATFORM_FAILURE can be or-ed with PLATFORM_NOT_FOUND or PLATFORM_END_OF_DIR.
 *  @see closeDir()
 *  @see opendir() [Linux]
 *  @see readdir() [Linux]
 *  @see FindFirstFile() [Windows]
 *  @see FindNextFile() [Windows]
*/
int iterateDir(cstring_t path, _dir* dir, string_t name, size_t nameSize);

/** Closes the dir object opened by iterateDir()
 * @return PLATFORM_SUCCESS or PLATFORM_FAILURE.
*/
int closeDir(_dir dir);

/** A struct representing a file memory mapping*/
typedef struct {
    /** A pointer to the memory mapping view */
    void* view;
    /** The size of the memory mapping */
    file_size_t size;
    /** [Windows] The size of the file */
    file_size_t totalSize;
    /** [Windows only] Handle for the memory mapping */
    HANDLE memMap;
} file_mapping_t;

/** Create a file memory mapping.
 * @param path The path to the file to be mapped.
 * @param mapData A pointer to the file_mapping_t struct to be filled with the memory mapping.
 * @return PLATFORM_SUCCESS or PLATFORM_FAILURE.
*/
int getFileMap(cstring_t path, file_mapping_t* mapData, file_size_t offset, size_t length);

/** Unmaps a file memory mapping 
 *  @return PLATFORM_SUCCESS or PLATFORM_FAILURE.
*/
int unmapMem(const file_mapping_t* map);

#endif