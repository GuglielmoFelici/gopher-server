#include <locale.h>
#include <stdio.h>
#include <windows.h>

#define LOG_ERR "Logger error\n"
#define MAX_LINE_SIZE 100
#define LOGGER_EVENT_NAME "logEvent"
#define LOG_FILE "logFile"

/* Quando viene segnalato l'evento logEvent, legge dallo stdInput (estremo in lettura della pipe di log) */
DWORD main(DWORD argc, LPSTR* argv) {
    char buff[MAX_LINE_SIZE] = "";
    HANDLE logEvent;
    if ((logEvent = OpenEvent(SYNCHRONIZE, FALSE, LOGGER_EVENT_NAME)) == NULL) {
        fprintf(stderr, LOG_ERR);
        exit(1);
    }
    HANDLE logFile;
    if ((logFile = CreateFile(LOG_FILE, GENERIC_READ | FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE) {
        fprintf(stderr, LOG_ERR);
        exit(1);
    }
    SetFilePointer(logFile, 0, NULL, FILE_END);
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
            printf("log %s\n", buff);
            if (!WriteFile(logFile, buff, bytesRead, &bytesWritten, NULL)) {
                fprintf(stderr, LOG_ERR);
            }
            printf("scritti %d\n", bytesWritten);
            if (!UnlockFile(logFile, 0, 0, fileSize.LowPart, fileSize.HighPart)) {
                fprintf(stderr, LOG_ERR);
            }
        } else {
            fprintf(stderr, LOG_ERR);
        }
    }
}