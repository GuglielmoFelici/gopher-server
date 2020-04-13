objects = src/main.o src/platform.o src/protocol.o src/server.o src/logger.o src/wingetopt.o
winHelperObjects = src/helpers/winGopherProcess.o src/platform.o src/protocol.o src/logger.o
winLoggerObjects = src/helpers/winLogger.o
CC = gcc

ifeq ($(OS),Windows_NT)     # is Windows_NT on XP, 2000, 7, Vista, 10...
	target = winserver.exe
	lib = lws2_32
	helpers = winLogger.exe winGopherProcess.exe
else
    target = linuxserver
	lib = pthread
endif

$(target) : $(objects) $(helpers)
	$(CC) $(CFLAGS) -o $@ $(objects) -$(lib)

debug : CFLAGS += -g
debug : $(objects) $(helpers) $(target)

winGopherProcess.exe : $(winHelperObjects)
	$(CC) $(CFLAGS) -o $@ $(winHelperObjects) -$(lib)

winLogger.exe : $(winLoggerObjects)
	$(CC) $(CFLAGS) -o $@ $(winLoggerObjects)

src/main.o : headers/globals.h headers/log.h headers/logger.h headers/platform.h headers/server.h headers/wingetopt.h
src/logger.o : headers/globals.h headers/logger.h
src/platform.o : headers/globals.h headers/protocol.h headers/platform.h  
src/protocol.o : headers/globals.h headers/datatypes.h headers/log.h headers/logger.h headers/platform.h
src/server.o : headers/globals.h headers/datatypes.h headers/log.h headers/logger.h headers/platform.h headers/protocol.h headers/server.h 
src/wingetopt.o : headers/globals.h headers/wingetopt.h
src/helpers/winGopherProcess.o : headers/logger.h headers/protocol.h
src/helpers/winLogger.o :

docs : doxy tex

tex : relazione.tex
	pdflatex -output-directory=documentation relazione.tex

doxy : Doxyfile
	doxygen

.PHONY : clean
clean :
	rm -f $(objects) $(target) *.exe
