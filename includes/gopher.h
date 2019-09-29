#ifndef GOPHER_H
#define GOPHER_H

#include "platform.h"

_string trimEnding(_string str);

void gopher(_cstring selector, _socket sock);

struct sendFileArgs {
    void* src;
    size_t size;
    _socket dest;
};

#endif
