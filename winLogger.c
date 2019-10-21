#include <fcntl.h>
#include <stdio.h>
#include <windows.h>

DWORD main(DWORD argc, LPSTR* argv) {
    HANDLE logFile;
    char buff[4096];
    DWORD bytesRead;
    DWORD bytesWritten;
    HANDLE logEvent;
    if ((logFile = CreateFile("logFile", FILE_APPEND_DATA, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE) {
        exit(1);
    }
    if ((logEvent = OpenEvent(SYNCHRONIZE, FALSE, "logEvent")) == NULL) {
        exit(1);
    }
    while (1) {
        WaitForSingleObject(logEvent, INFINITE);
        if (!ReadFile(GetStdHandle(STD_INPUT_HANDLE), buff, sizeof(buff), &bytesRead, NULL)) {
            CloseHandle(logFile);
            exit(1);
        } else {
            WriteFile(logFile, buff, bytesRead, &bytesWritten, NULL);
        }
    }
    // munmap(mutexShare, sizeof(pthread_mutex_t));
    // munmap(condShare, sizeof(pthread_cond_t));
    // free(mutexShare);
    // free(condShare);
    CloseHandle(logFile);
}