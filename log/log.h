#include "../everything.h"

#define CONFIG_ERROR "Impossibile leggere il file di configurazione, verranno usate le impostazioni di default.\n"
#define LOG_ERROR "Errore nel logging\n"

#define INFO "log/info"
#define ERR "log/err"
#define DEBUG "log/debug"

char* currentTime();

bool initLog();

void err(const char* message, char *level, int code);

void _log(const char *message, const char *level);