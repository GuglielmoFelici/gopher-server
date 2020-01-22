gcc -g src/*.c -o winserver -lws2_32
gcc -g src/helpers/winGopherProcess.c src/platform.c src/protocol.c src/logger.c -o src/helpers/winGopherProcess.exe -lws2_32
gcc -g src/helpers/winLogger.c -o src/helpers/winLogger.exe
