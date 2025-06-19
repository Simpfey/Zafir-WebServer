CC = gcc
CFLAGS = -Wall -pthread

SRC = main.c server.c handler.c mime.c cache.c ratelimit.c log.c
OBJ = $(SRC:.c=.o)

all: server

server: $(OBJ)
	$(CC) $(CFLAGS) -o server $(OBJ) -lz

clean:
	rm -f *.o server
