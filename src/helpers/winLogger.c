#include <locale.h>
#include <stdio.h>
#include <windows.h>

#define LOG_ERR "WARNING - Logger - Fatal error, maybe already running?\n"
#define LOGGER_EVENT_NAME "logEvent"
#define MAX_LINE_SIZE 200

/* Richiede la rilettura del file di configurazione */
static BOOL sigHandler(DWORD signum) {
    return signum == CTRL_BREAK_EVENT;
}

/* Quando viene segnalato l'evento logEvent, legge dallo stdInput (estremo in lettura della pipe di log) */
DWORD main(DWORD argc, LPSTR* argv) {
    char buff[MAX_LINE_SIZE];
    HANDLE logEvent;
    if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)sigHandler, TRUE)) {
        exit(1);
    }
    if ((logEvent = OpenEvent(SYNCHRONIZE, FALSE, LOGGER_EVENT_NAME)) == NULL) {
        fprintf(stderr, LOG_ERR);
        exit(1);
    }
    HANDLE logFile;
    if (INVALID_HANDLE_VALUE == (logFile = CreateFile(argv[1], GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL))) {
        fprintf(stderr, LOG_ERR);
        exit(1);
    }
    while (1) {
        WaitForSingleObject(logEvent, INFINITE);
        DWORD bytesRead;
        if (!ReadFile(GetStdHandle(STD_INPUT_HANDLE), buff, sizeof(buff), &bytesRead, NULL)) {
            fprintf(stderr, LOG_ERR);
            continue;
        }
        OVERLAPPED ovlp;
        memset(&ovlp, 0, sizeof(ovlp));
        LARGE_INTEGER fileSize;
        if (!GetFileSizeEx(logFile, &fileSize)) {
            fprintf(stderr, LOG_ERR);
            continue;
        };
        if (LockFileEx(logFile, LOCKFILE_EXCLUSIVE_LOCK, 0, fileSize.LowPart, fileSize.HighPart, &ovlp)) {
            DWORD bytesWritten;
            if (!WriteFile(logFile, buff, bytesRead, &bytesWritten, NULL)) {
                fprintf(stderr, LOG_ERR);
            }
            if (!UnlockFileEx(logFile, 0, fileSize.LowPart, fileSize.HighPart, &ovlp)) {
                fprintf(stderr, LOG_ERR);
            }
        } else {
            fprintf(stderr, LOG_ERR);
        }
    }
}