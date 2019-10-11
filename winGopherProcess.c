#include "includes/gopher.h"
#include "includes/platform.h"

#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <windows.h>

DWORD main(DWORD argc, LPSTR* argv) {
    AllocConsole();

    HANDLE handle_out = GetStdHandle(STD_OUTPUT_HANDLE);
    int hCrt = _open_osfhandle((long)handle_out, _O_TEXT);
    FILE* hf_out = _fdopen(hCrt, "w");
    setvbuf(hf_out, NULL, _IONBF, 1);
    *stdout = *hf_out;

    HANDLE handle_in = GetStdHandle(STD_INPUT_HANDLE);
    hCrt = _open_osfhandle((long)handle_in, _O_TEXT);
    FILE* hf_in = _fdopen(hCrt, "r");
    setvbuf(hf_in, NULL, _IONBF, 128);
    *stdin = *hf_in;

    // use the console just like a normal one - printf(), getchar(), ...

    SOCKET sock;
    startup();
    LPSTR a = argv[0];
    LPSTR b = argv[1];
    //printf("%s\n", argv[0]);
    //printf("%s\n", argv[1]);
    sock = atoi(argv[1]);
    printf("%d\n", sock);
    printf("%d\n", send(sock, "ciao", 4, 0));
    printf("%d\n", WSAGetLastError());
}