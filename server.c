#include "platform.h"
#include "errors.h"
#include <stdio.h>

#define CONFIG_FILE "config"

int main(int argc, char** argv) {
    
    startup();
    /* Configuration */
    struct config options;
    initConfig(&options);
    if (!readConfig(CONFIG_FILE, &options)) {
        fprintf(stderr, CONFIG_ERROR);
        perror(CONFIG_FILE);
    }

}