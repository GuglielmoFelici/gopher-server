#ifndef LOG_H
#define LOG_H

#include "datatypes.h"
#include "everything.h"
#include "platform.h"

#define _CONFIG_ERROR "Impossibile leggere il file di configurazione, verranno usate le impostazioni di default."
#define _LOG_ERROR "Errore nel logging"
#define _SOCKET_ERROR "Errore nella creazione del socket"
#define _WINSOCK_ERROR "Errore nell'inizializzazione di Winsock"
#define _REUSE_ERROR "Errore nel settaggio di REUSEADDR"
#define _BIND_ERROR "Errore in fase di binding"
#define _LISTEN_ERROR "Errore in fase di listen"
#define _NONBLOCK_ERR "Impossibile settare il socket come nonblocking"
#define _SELECT_ERR "Errore nella select"
#define _FORK_ERR "Errore nella fork"
#define _THREAD_ERR "Errore nella creazione di un nuovo thread"

#define _EXEC_ERR "Errore nell'esecuzione del comando"
#define _READDIR_ERR "Errore nella lettura della directory"
#define _READFILE_ERR "Errore nella lettura del file"

#define _SOCKET_OPEN "Server socket aperto"
#define _SOCKET_LISTENING "Socket in ascolto"

#define INFO "log/info"
#define ERR "log/err"
#define DEBUG "log/debug"
#define TEST "log/test"

_string currentTime();

int initLog();

void err(const _string message, const _string level, bool stderror, int code);

void _log(const _string message, const _string level, bool stderror);

#endif