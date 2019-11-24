#include <stdio.h>
#include <windows.h>

HANDLE logFile;
HANDLE logEvent;

BOOL sigHandler(DWORD signum) {
    CloseHandle(logEvent);
    exit(0);
}

/* Quando riceve un evento logEvent, legge dallo stdInput (estremo in lettura della pipe di log) */
DWORD main(DWORD argc, LPSTR* argv) {
    char buff[4096];
    DWORD bytesRead;
    DWORD bytesWritten;
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)sigHandler, TRUE);
    if ((logEvent = OpenEvent(SYNCHRONIZE, FALSE, "logEvent")) == NULL) {
        printf("Errore starting logger\n.");
        exit(1);
    }
    while (1) {
        WaitForSingleObject(logEvent, INFINITE);
        if ((logFile = CreateFile("logFile", FILE_APPEND_DATA, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE) {
            printf("Impossibile loggare il trasferimento - Impossibile aprire logFile\n");
        } else if (!ReadFile(GetStdHandle(STD_INPUT_HANDLE), buff, sizeof(buff), &bytesRead, NULL)) {
            printf("Impossibile loggare il trasferimento\n");
            CloseHandle(logFile);
        } else {
            WriteFile(logFile, buff, bytesRead, &bytesWritten, NULL);
            CloseHandle(logFile);
        }
    }
    CloseHandle(logFile);
    printf("canhio???");
}