#ifndef CONSTANTS_H
#define CONSTANTS_H

#if defined(_WIN32)  // WINDOWS

// #ifndef _WIN32_WINNT  // TODO ??
// #define _WIN32_WINNT 0x0501
// #endif
#define HELPER_PATH "src\\helpers\\winGopherProcess.exe"

#else  // LINUX

#define INVALID_SOCKET -1
#define SOCKET_ERROR -1

#endif

#define IP_ADDRESS_LENGTH 16

/* Config */

#endif