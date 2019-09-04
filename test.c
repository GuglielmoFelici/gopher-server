#include "common.h"
#include "errors.h"

int main() {
    struct config opt;
    if (!readConfig("confaig", &opt)) {
        fprintf(stderr, CONFIG_ERROR);
        perror("confaig");
    }
}