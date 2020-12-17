CC =gcc
CCFLAGS= -Wall -Wextra

all: client server 
client: client.o
	$(CC) -g -o client client.o
server: server.o
	$(CC) -g -o server server.o
client.o: client.c 
	$(CC) -g -c client.c header.h
server.o: server.c 
	$(CC) -g -c server.c header.h
clean: 
	rm client server client.o server.o 
