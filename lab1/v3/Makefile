CC=gcc
CFLAGS=-I.

OBJ=fifocli-v1.o fifoserver-v1.o
 
all: $(OBJ) clean_fifos

%.o: %.c
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY:
clean_fifos:
	rm -f server_queue
	rm -f client_queue
