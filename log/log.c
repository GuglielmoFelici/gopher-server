#include "log.h"

char* currentTime() {
    time_t rawtime;
    struct tm *timeinfo;
    time( &rawtime );
    timeinfo = localtime( &rawtime );
    return asctime(timeinfo);
}

void err(const char* message, char *level, int code) {
    _log(message, level);
    exit(code);
}

bool initLog() {
    char* logs[3] = {INFO, DEBUG, ERR};
    FILE* logFile;
    for (int i=0; i<3; i++) {
        logFile = fopen(logs[i], "a");
        if (logFile == NULL) {
            return false;
        }
        fprintf(logFile, currentTime());
        fclose(logFile);
    }
    return true;
}

void _log(const char *message, const char *level) {
    FILE *out;
    if ((out = fopen(level, "a")) == NULL) {
        fprintf(stderr, LOG_ERROR);
        perror(level);
    }
    fprintf(out, message);
    fclose(out);
}