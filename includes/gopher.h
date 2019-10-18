#ifndef GOPHER_H
#define GOPHER_H

#include "platform.h"

extern _pipe logPipe;

_string trimEnding(_string str);

_thread gopher(_cstring selector, _socket sock);

void errorRoutine(void* sock);
struct sendFileArgs {
    void* src;
    size_t size;
    _socket dest;
};

void* sendFile(void* sendFileArgs);

#endif
