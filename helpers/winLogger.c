#include <stdio.h>
#include <windows.h>

HANDLE logFile;
HANDLE logEvent;

BOOL sigHandler(DWORD signum) {
    CloseHandle(logFile);
    CloseHandle(logEvent);
    exit(0);
}

/* Quando riceve un evento logEvent, legge dallo stdInput (estremo in lettura della pipe di log) */
DWORD main(DWORD argc, LPSTR* argv) {
    char buff[4096] = "";
    DWORD bytesRead;
    DWORD bytesWritten;
    DWORD sizeHigh;
    DWORD sizeLow;
    OVERLAPPED ovlp;
    memset(&ovlp, 0, sizeof(ovlp));
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)sigHandler, TRUE);
    if ((logEvent = OpenEvent(SYNCHRONIZE, FALSE, "logEvent")) == NULL) {
        fprintf(stderr, "Error starting logger\n.");
        exit(1);
    }
    if ((logFile = CreateFile("logFile", GENERIC_READ | FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Error opening logFile: %d\n", GetLastError());
        exit(1);
    }
    SetFilePointer(logFile, 0, NULL, FILE_END);
    while (1) {
        WaitForSingleObject(logEvent, INFINITE);
        if (!ReadFile(GetStdHandle(STD_INPUT_HANDLE), buff, sizeof(buff), &bytesRead, NULL)) {
            fprintf(stderr, "Can't read from pipe: %d\n", GetLastError());
            continue;
        }
        sizeLow = GetFileSize(logFile, &sizeHigh);
        if (LockFileEx(logFile, LOCKFILE_EXCLUSIVE_LOCK, 0, sizeLow, sizeHigh, &ovlp)) {
            // TODO prova sleep
            if (!WriteFile(logFile, buff, bytesRead, &bytesWritten, NULL)) {
                fprintf(stderr, "Error writing logFile\n");
            }
            if (!UnlockFile(logFile, 0, 0, sizeLow, sizeHigh)) {
                fprintf(stderr, "Logger - error unlocking file %d\n", GetLastError());
            }
        } else {
            fprintf(stderr, "Logger - Error locking file: %d\n", GetLastError());
        }
    }
}