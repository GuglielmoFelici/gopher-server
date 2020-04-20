/** \file datatypes.h
 * Portable data types encapsulation. 
 * Includes:\n 
 * socket_t\n 
 * string_t\n 
 * cstring_t - Constant string\n 
 * thread_t\n 
 * pipe_t\n 
 * event_t\n 
 * _dir - Directory object.\n 
 * mutex_t\n 
 * cond_t - Condition variable type\n 
 * proc_id_t - Process id type\n 
 * sig_atomic - [Linux] Variable signal-wise atomic
 * @see startThread()
 * @see logger_t
 * @see iterateDir()
 */

#ifndef DATATYPES_H
#define DATATYPES_H

#if defined(_WIN32)
#include <stdint.h>
#include <windows.h>

typedef SOCKET socket_t;
typedef LPSTR string_t;
typedef LPCSTR cstring_t;
typedef HANDLE thread_t, pipe_t, event_t, _dir, mutex_t, cond_t;
typedef DWORD proc_id_t, sig_atomic;

#else

#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
typedef int socket_t, event_t;
typedef int pipe_t;
typedef char* string_t;
typedef const char* cstring_t;
typedef pthread_t thread_t;
typedef void* (*LPTHREAD_START_ROUTINE)(void*);
typedef sig_atomic_t sig_atomic;
typedef DIR* _dir;
typedef pthread_mutex_t mutex_t;
typedef pthread_cond_t cond_t;
typedef pid_t proc_id_t;

#endif

#endif