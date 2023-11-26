
CC		= g++
PROGS	= server
UID		= $(shell stat -c "%u:%g" /build/Makefile 2>/dev/null || stat -c "%u:%g" ./Makefile 2> /dev/null)

all: server

%.o: %.cpp
	$(CC) -c -g -Wall $<

server: server.o
	$(CC) -std=c++17 -o $@ -static $^
	chown -R "$(UID)" *.o server

clean:
	rm -f *.o $(PROGS)
