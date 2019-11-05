gcc -g platform.c server.c protocol.c log/*.c config/*.c -o winserver -lws2_32
gcc -g winGopherProcess.c platform.c log/log.c config/config.c protocol.c -o winGopherProcess.exe -lws2_32
gcc -g winLogger.c -o winLogger.exe
