APP = gpmc-timings
SOURCES = main.c
CC = gcc
CFLAGS = -Wall -O2

all: $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $(APP)

clean:
	-rm -f $(APP)

.PHONY: all clean
