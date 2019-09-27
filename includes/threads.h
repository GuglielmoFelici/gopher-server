#ifndef THREADS_H
#define THREADS_H

#include "platform.h"

bool isDirectory(_fileData* file);

char gopherType(const _file* file);

void gopherResponse(LPCSTR path, _string response);

void* task(void* args);

struct threadArgs {
    _socket socket;
    struct config options;
};

#endif
