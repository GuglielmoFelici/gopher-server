#include "../includes/log.h"

char* currentTime() {
    time_t rawtime;
    struct tm *timeinfo;
    time( &rawtime );
    timeinfo = localtime( &rawtime );
    return asctime(timeinfo);
}

void err(const char* message, char *level, bool stderror, int code) {
    _log(message, level, stderror);
    exit(code);
}

int initLog() {
    char* logs[3] = {INFO, DEBUG, ERR};
    FILE* logFile;
    for (int i=0; i<3; i++) {
        logFile = fopen(logs[i], "a");
        if (logFile == NULL) {
            return errno;
        }
        fprintf(logFile, currentTime());
        fclose(logFile);
    }
    return 0;
}

void _log(const char *message, const char *level, bool stderror) {
    FILE *out;
    if ((out = fopen(level, "a")) == NULL) {
        fprintf(stderr, _LOG_ERROR);
        perror(level);
    }
    fprintf(out, "%s\n", message);
    if (stderror) {
        fprintf(out, "Descrizione errore: %s\n", strerror(errno));
    }
    fclose(out);
}