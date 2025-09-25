CC = gcc
CFLAGS = -g -Wall -pedantic
LDFLAGS = -lmicrohttpd

BIN = alguiendespacho

$(BIN): main.c
	$(CC) -o $@ $(CFLAGS) $? $(LDFLAGS)

.PHONY: clean
clean:
	rm $(BIN)

