#ifndef WINVER
    #define WINVER 0x0603
#endif

#ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0603
#endif

#ifndef _WIN32_IE
    #define _WIN32_IE 0x0700
#endif
#include <windows.h>
#include <stdio.h>

int main(int argc, char** argv) {
	printf("%d\n", AttachConsole(atoi(argv[1])));
	printf("%d\n", GenerateConsoleCtrlEvent(CTRL_C_EVENT, atoi(argv[1])));
}