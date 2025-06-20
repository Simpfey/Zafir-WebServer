CC = gcc
CFLAGS = -Wall -pthread

SRC = main.c server.c handler.c mime.c cache.c ratelimit.c log.c header.c path.c gzip.c config.c https.c ssl.c
OBJ = $(SRC:.c=.o)

all: server

server: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) -lz -lssl -lcrypto -I/usr/include/openssl

clean:
	rm -f *.o *.d server

.PHONY: all clean

# Optional: dependency generation
DEP = $(SRC:.c=.d)
-include $(DEP)

%.o: %.c
	$(CC) $(CFLAGS) -MMD -c $< -o $@
