CC=gcc
objects = src/main.o src/logger.o src/platform.o src/protocol.o src/server.o src/wingetopt.o

linuxserver : $(objects)
	$(CC) -o linuxserver -pthread $(objects)

main.o : headers/log.h headers/logger.h headers/platform.h headers/server.h headers/wingetopt.h    
logger.o : headers/logger.h 
platform.o : headers/protocol.h headers/platform.h  
protocol.o : headers/datatypes.h headers/log.h headers/logger.h headers/platform.h 
server.o : headers/datatypes.h headers/log.h headers/logger.h headers/platform.h headers/protocol.h headers/server.h 
wingetopt.o : headers/wingetopt.h

.PHONY : clean
clean :
	rm linuxserver $(objects)
