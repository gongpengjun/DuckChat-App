#
# DuckChat Makefile
# Author: Cole Vikupitz
# Last Modified: 11/3/2017
#


FILES=Makefile client.c server.c raw.h raw.c duckchat.h\
	hashmap.h hashmap.c linkedlist.h linkedlist.c README.md

CC=gcc
CFLAGS=-Wall -W -g
OBJECTS=client.o server.o raw.o hashmap.o linkedlist.o
EXECS=client server


all: $(EXECS)

client: client.o raw.o
	$(CC) $(CFLAGS) client.o raw.o -o client

server: server.o hashmap.o linkedlist.o
	$(CC) $(CFLAGS) server.o hashmap.o linkedlist.o -o server

tarfile:
	mkdir DuckChat/
	cp $(FILES) DuckChat/
	tar -zcvf DuckChat.tgz DuckChat/
	tar -ztvf DuckChat.tgz
	rm -fr DuckChat/

help:
	@echo "Possible targets for make:"
	@echo "    all     - Compiles both client & server executables."
	@echo "    client  - Compiles the client executable."
	@echo "    server  - Compiles the server executable."
	@echo "    tarfile - Creates a tar archive of the project."
	@echo "    help    - Display list of possible make targets."
	@echo "    clean   - Removes project executables and object files."

clean:
	rm -f $(OBJECTS) $(EXECS)

client.o: client.c duckchat.h raw.h
server.o: server.c duckchat.h
raw.o: raw.c raw.h
hashmap.o: hashmap.c hashmap.h
linkedlist.o: linkedlist.c linkedlist.h
