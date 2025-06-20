CC = gcc
CFLAGS = -Wall -pthread

SRC = main.c server.c handler.c mime.c cache.c ratelimit.c log.c header.c path.c gzip.c
OBJ = $(SRC:.c=.o)

all: server

server: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) -lz

clean:
	rm -f *.o *.d server

.PHONY: all clean

# Optional: dependency generation
DEP = $(SRC:.c=.d)
-include $(DEP)

%.o: %.c
	$(CC) $(CFLAGS) -MMD -c $< -o $@
