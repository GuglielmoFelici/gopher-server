#include "includes/gopher.h"
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

    // use the console just like a normal one - printf(), getchar(), ...

    SOCKET sock;
    HANDLE thread;
    char message[256];
    startup();
    sock = atoi(argv[1]);
    recv(sock, message, sizeof(message), 0);
    trimEnding(message);
    printf("wingopher - received : %s;\n", message);
    thread = gopher(message, sock);
    if (thread != NULL) {
        WaitForSingleObject(thread, 10000);
    }
    closeSocket(sock);
    printf("Exiting process...\n");
    return 0;
}