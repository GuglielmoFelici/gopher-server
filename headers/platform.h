/** \file platform.h
 * Portable utility functions.
 * @file platform.c
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#if defined(_WIN32)
#include <windows.h>
#define LOG_DEBUG 0
#define LOG_INFO 1
#define LOG_WARNING 2
#define LOG_ERR 3

#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <syslog.h>
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

#include <stdbool.h>
#include <stdlib.h>
#include "datatypes.h"

#define PLATFORM_SUCCESS 0x0000
#define PLATFORM_FAILURE 0x0001
#define PLATFORM_FILE_ERR 0x0001
#define PLATFORM_ISFILE 0x0002
#define PLATFORM_ISDIR 0x0004
#define PLATFORM_NOT_FOUND 0x0008
#define PLATFORM_END_OF_DIR 0x0010

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
 * @param level The importance level. Can be LOG_DEBUG, LOG_INFO, LOG_WARNING or LOG_ERR.
*/
void logMessage(cstring_t message, int level);

/** @return the current system error relative to the sockets */
int sockErr();

/** Closes the socket s.
 * @return PLATFORM_SUCCESS or PLATFORM_FAILURE.
*/
int closeSocket(socket_t s);

/** Tries to send up to length bytes of data to the socket s.
 * @return PLATFORM_SUCCESS or PLATFORM_FAILURE.
*/
int sendAll(socket_t s, cstring_t data, int length);

/** @see inet_ntoa [Windows]
 *  @see inet_ntop [Linux]
*/
cstring_t inetNtoa(const struct in_addr* addr, void* dst, size_t size);

/** Retrieves the attributes of a file.
 * @return PLATFORM_ISFILE or PLATFORM_ISDIR. Upon error returns PLATFORM_FILE_ERR, or-ed with PLATFORM_NOT_FOUND if the file was not found.
*/
int fileAttributes(cstring_t path);

/** @return The size of the file path, or -1 upon failure.
*/
int getFileSize(cstring_t path);

/** Reads the next files of a directory and copies its relative path in name.
 *  If dir is NULL, opens the directory path and initializes dir for further calls.
 *  If dir is non-NULL, path is ignored (thus can be NULL), and the scan continues from the previouse iterateDir() call.
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
    /** A pointer to the memory mapping */
    void* view;
    /** The size of the memory mapping */
    int size;
} file_mapping_t;

/** Create a file memory mapping.
 * @param path The path to the file to be mapped.
 * @param mapData A pointer to the file_mapping_t struct to be filled with the memory mapping.
 * @return PLATFORM_SUCCESS or PLATFORM_FAILURE.
*/
int getFileMap(cstring_t path, file_mapping_t* mapData);

/** Unmaps a file memory mapping 
 *  @return PLATFORM_SUCCESS or PLATFORM_FAILURE.
*/
int unmapMem(void* addr, size_t len);

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

/** Transforms the current process into a daemon process. 
 * @return PLATFORM_SUCCESS or PLATFORM_FAILURE.
*/
int daemonize();

#endif