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
#include <windows.h>

typedef SOCKET socket_t;
typedef LPSTR string_t;
typedef LPCSTR cstring_t;
typedef HANDLE thread_t, pipe_t, event_t, _dir, mutex_t, cond_t, semaphore_t;
typedef DWORD proc_id_t, sig_atomic;
typedef long long file_size_t;  // TODO long long è il tipo giusto?

#else

#include <dirent.h>
#include <errno.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
typedef int socket_t, event_t, pipe_t;
typedef char* string_t;
typedef const char* cstring_t;
typedef pthread_t thread_t;
typedef pthread_mutex_t mutex_t;
typedef pthread_cond_t cond_t;
typedef void* (*LPTHREAD_START_ROUTINE)(void*);
typedef sem_t semaphore_t;
typedef sig_atomic_t sig_atomic;
typedef DIR* _dir;
typedef pid_t proc_id_t;
typedef long long file_size_t;

#endif

#endif