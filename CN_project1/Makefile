CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=gnu17 -Iinclude
LFLAGS  =

.PHONY: all clean ppcbc ppcbs

all: ppcbc ppcbs

ppcbc: src/ppcbc.o src/client_tcp.o src/client_udp.o src/client_udpr.o src/common.o src/err.o
	$(CC) $(LFLAGS) -o ppcbc $^

ppcbs: src/ppcbs.o src/server_tcp.o src/server_udp.o src/server_udpr.o src/server_udp_udpr.o src/common.o src/err.o
	$(CC) $(LFLAGS) -o ppcbs $^

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o ppcbc ppcbs
