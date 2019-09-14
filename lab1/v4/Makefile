CC=gcc

all: clean_all client server

client:
	$(CC) -g -o client fifocli-v3.c

server:
	$(CC) -g -o server fifoser-v3.c

.PHONY:
clean_all:
	rm -f server_queue
	rm -f client_queue
	rm -f client
	rm -f server
