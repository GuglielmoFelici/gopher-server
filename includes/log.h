#ifndef LOG_H
#define LOG_H

#define _GNU_SOURCE
#include "datatypes.h"
#include "gopher.h"

#define _GENERIC_ERR "Errore generico"
#define _CONFIG_ERR "Impossibile leggere il file di configurazione, verranno usate le impostazioni di default."
#define _LOG_ERR "Errore nel logging"
#define _SOCKET_ERR "Errore nella creazione del socket"
#define _STARTUP_ERR "Errore nell'inizializzazione"
#define _REUSE_ERR "Errore nel settaggio di REUSEADDR"
#define _BIND_ERR "Errore in fase di binding"
#define _LISTEN_ERR "Errore in fase di listen"
#define _NONBLOCK_ERR "Impossibile settare il socket come nonblocking"
#define _SELECT_ERR "Errore nella select"
#define _FORK_ERR "Errore nella fork"
#define _DAEMON_ERR "Errore nella demonizzazione del processo"
#define _THREAD_ERR "Errore nella creazione di un nuovo thread"
#define _EXEC_ERR "Errore nell'esecuzione del comando"
#define _READDIR_ERR "Errore nella lettura della directory"
#define _READFILE_ERR "Errore nella lettura del file"
#define _MAPFILE_ERR "Errore nel mapping del file"
#define _CLOSE_SOCKET_ERR "Errore nella chiusura del socket"
#define _ALLOC_ERR "Errore nell'allocazione della memoria'"

#define _SOCKET_OPEN "Server socket aperto"
#define _SOCKET_LISTENING "Socket in ascolto"

#define WARN "log/warn"
#define ERR "log/err"

_string currentTime();

int initLog();

void _err(const _string message, const _string level, bool stderror, int code);

void _log(const _string message, const _string level, bool stderror);

#endif