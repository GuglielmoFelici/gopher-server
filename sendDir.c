#include "headers/gopher.h"

typedef DIR* _dir;

int sendDirLinux(_string path) {
    // iter = opendir(path)
    // data = findNext
}

int sendDirWin(_string path) {
    // iter = findFirstFile(path, &data)
    // bool = findNext(&data)
}

int mySendDir(_string path) {
    // iter = first(path, &data);P
    // bool = next(&data)
}