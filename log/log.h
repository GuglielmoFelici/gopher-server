#include "../everything.h"

#define _CONFIG_ERROR "Impossibile leggere il file di configurazione, verranno usate le impostazioni di default."
#define _LOG_ERROR "Errore nel logging"
#define _SOCKET_ERROR "Errore nella creazione del socket"
#define _WINSOCK_ERROR "Errore nell'inizializzazione di Winsock"

#define INFO "log/info"
#define ERR "log/err"
#define DEBUG "log/debug"

char* currentTime();

int initLog();

void err(const char* message, char *level, bool stderror, int code);

void _log(const char *message, const char *level, bool stderror);