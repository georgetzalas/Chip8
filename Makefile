CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -ggdb
LDFLAGS = -lSDL2

SRC = chip8.c
EXECUTABLE = chip8

all: $(EXECUTABLE)

$(EXECUTABLE): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(EXECUTABLE)

.PHONY: clean