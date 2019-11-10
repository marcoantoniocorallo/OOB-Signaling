CC = gcc
CFLAGS = -std=c99 -Wall -g

.PHONY: clean test

all: server client supervisor

supervisor: main.c icl_hash.o
	$(CC) $(CFLAGS) main.c -o $@ -I. icl_hash.o

server:	server.c
	$(CC) $(CFLAGS) server.c -o $@ -I. -pthread

client: client.c
	$(CC) $(CFLAGS) -I. client.c workerMask_t.h -o $@

icl_hash.o:	icl_hash.c
	$(CC) $(CFLAGS) -I. -c -o $@ $<

clean:
	rm -f OOB* server client supervisor *.txt *.o

test:
	bash test.sh