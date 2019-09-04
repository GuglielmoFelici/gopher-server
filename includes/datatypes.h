#if defined(_WIN32)
#include <windows.h>

typedef DWORD _int;
typedef SOCKET _socket;


#else

typedef DWORD _int;
typedef int _socket;

#endif