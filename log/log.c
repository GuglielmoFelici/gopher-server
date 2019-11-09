#include "../includes/log.h"

_string currentTime() {
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    return asctime(timeinfo);
}

void _err(const _string message, const _string level, bool stderror, int code) {
    _log(message, level, stderror);
    exit(code);
}

int initLog() {
    _string logs[3] = {INFO, DEBUG, ERR};
    for (int i = 0; i < 3; i++) {
        FILE *logFile = fopen(logs[i], "a");
        if (logFile == NULL) {
            return errno;
        }
        fputs(currentTime(), logFile);
        fclose(logFile);
    }
    return 0;
}

void _log(const _string message, const _string level, bool stderror) {
    FILE *out;
    if ((out = fopen(level, "a")) == NULL) {
        fprintf(stderr, _LOG_ERR);
        return;
    }
    fprintf(out, "%s\n", message);
    if (stderror) {
        char error[50];
        errorString(error);
        fprintf(out, "Descrizione errore: %s\n", error);
    }
    fclose(out);
}