#include <locale.h>
#include <stdio.h>
#include <windows.h>

HANDLE logFile;
HANDLE logEvent;

void _logErr(LPCSTR message) {
    wchar_t buf[256];
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   buf, 256, NULL);
    if (FreeConsole()) {
        AllocConsole();
    } else {
        exit(-1);
    }
    fprintf(stderr, "*** GOPHER LOGGER ***\n%s\n", message);
    fwprintf(stderr, buf);
    fprintf(stderr, "\nServer will work, but logging will be disabled\nUse CTRL-C to close the logger.\n");
    Sleep(INFINITE);  // Lascio all'utente la possibilità di leggere l'errore
}

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
    setlocale(LC_ALL, "");
    memset(&ovlp, 0, sizeof(ovlp));
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)sigHandler, TRUE);
    if ((logEvent = OpenEvent(SYNCHRONIZE, FALSE, "logEvent")) == NULL) {
        _logErr("Error starting logger\n.");
        exit(1);
    }
    if ((logFile = CreateFile("logFile", GENERIC_READ | FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE) {
        _logErr("Error opening logFile");
        exit(1);
    }
    SetFilePointer(logFile, 0, NULL, FILE_END);
    while (1) {
        WaitForSingleObject(logEvent, INFINITE);
        if (!ReadFile(GetStdHandle(STD_INPUT_HANDLE), buff, sizeof(buff), &bytesRead, NULL)) {
            _logErr("Can't read from pipe");
            continue;
        }
        sizeLow = GetFileSize(logFile, &sizeHigh);
        if (LockFileEx(logFile, LOCKFILE_EXCLUSIVE_LOCK, 0, sizeLow, sizeHigh, &ovlp)) {
            // TODO prova sleep
            if (!WriteFile(logFile, buff, bytesRead, &bytesWritten, NULL)) {
                _logErr("Error writing logFile");
            }
            if (!UnlockFile(logFile, 0, 0, sizeLow, sizeHigh)) {
                _logErr("Logger - error unlocking file");
            }
        } else {
            _logErr("Logger - Error locking file");
        }
    }
}