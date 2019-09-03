#include "platform.h"

#include <stdio.h>

#define CONFIG_FILE "config"

int main(int argc, char** argv) {
    
    startup();
    /* Configuration */
    struct config options;
    initConfig(&options);
    readConfig(CONFIG_FILE, &options);

}