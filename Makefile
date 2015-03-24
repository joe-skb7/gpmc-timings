APP = gpmc-timings
SOURCES = main.c

default: $(SOURCES)
	$(CC) -Wall -O2 $(SOURCES) -o $(APP)

clean:
	-rm -f $(APP)

.PHONY: default clean
