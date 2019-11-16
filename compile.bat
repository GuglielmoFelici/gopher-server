gcc -g *.c -o winserver -lws2_32
gcc -g helpers/winGopherProcess.c platform.c protocol.c -o helpers/winGopherProcess.exe -lws2_32
gcc -g helpers/winLogger.c -o helpers/winLogger.exe
