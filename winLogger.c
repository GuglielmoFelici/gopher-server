#include <fcntl.h>
#include <stdio.h>
#include <windows.h>

DWORD main(DWORD argc, LPSTR* argv) {
    // AllocConsole();

    // HANDLE handle_out = GetStdHandle(STD_OUTPUT_HANDLE);
    // int hCrt = _open_osfhandle((long)handle_out, _O_TEXT);
    // FILE* hf_out = _fdopen(hCrt, "w");
    // setvbuf(hf_out, NULL, _IONBF, 1);
    // *stdout = *hf_out;

    // HANDLE handle_in = GetStdHandle(STD_INPUT_HANDLE);
    // hCrt = _open_osfhandle((long)handle_in, _O_TEXT);
    // FILE* hf_in = _fdopen(hCrt, "r");
    // setvbuf(hf_in, NULL, _IONBF, 128);
    // *stdin = *hf_in;

    HANDLE logFile;
    char buff[4096];
    DWORD bytesRead;
    DWORD bytesWritten;
    if ((logFile = CreateFile("logFile", FILE_APPEND_DATA, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE) {
        exit(1);
    }
    while (1) {
        //wait
        if (!ReadFile(GetStdHandle(STD_INPUT_HANDLE), buff, sizeof(buff), &bytesRead, NULL)) {
            CloseHandle(logFile);
            exit(1);
        } else if (!strcmp(buff, "KILL")) {
            break;
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