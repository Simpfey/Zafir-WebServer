CC = gcc
CFLAGS = -Wall -pthread -I/usr/include/openssl

SRC_DIR = ../src
BIN_DIR = .

SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(patsubst $(SRC_DIR)/%.c, $(BIN_DIR)/%.o, $(SRC))
DEP = $(OBJ:.o=.d)

TARGET = $(BIN_DIR)/server

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) -lz -lssl -lcrypto

# Ensure bin directory exists
$(BIN_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -MMD -c $< -o $@

clean:
	rm -f $(BIN_DIR)/*.o $(BIN_DIR)/*.d $(TARGET)

.PHONY: all clean

# Include dependency files
-include $(DEP)
