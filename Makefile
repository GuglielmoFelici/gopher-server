objects = src/main.o src/platform.o src/protocol.o src/server.o src/logger.o src/wingetopt.o
winHelperObjects = src/helpers/winGopherProcess.o src/platform.o src/protocol.o src/logger.o
winLoggerObjects = src/helpers/winLogger.o
CC=gcc

ifeq ($(OS),Windows_NT)     # is Windows_NT on XP, 2000, 7, Vista, 10...
	target = winserver.exe
	lib = lws2_32
	helpers = src/helpers/winLogger.exe src/helpers/winGopherProcess.exe
else
    target = linuxserver
	lib = pthread
endif

$(target) : $(objects) $(helpers)
	$(CC) -o $@ $(objects) -$(lib)

src/helpers/winGopherProcess.exe : $(winHelperObjects)
	$(CC) -o $@ $(winHelperObjects) -$(lib)

src/helpers/winLogger.exe : $(winLoggerObjects)
	$(CC) -o $@ $(winLoggerObjects)

main.o : headers/log.h headers/logger.h headers/platform.h headers/server.h headers/wingetopt.h    
logger.o : headers/logger.h
platform.o : headers/protocol.h headers/platform.h  
protocol.o : headers/datatypes.h headers/log.h headers/logger.h headers/platform.h 
server.o : headers/datatypes.h headers/log.h headers/logger.h headers/platform.h headers/protocol.h headers/server.h 
wingetopt.o : headers/wingetopt.h
winGopherProcess.o : headers/logger.h headers/protocol.h
winLogger.o :


.PHONY : clean
clean :
	rm -f $(objects) $(target) 
