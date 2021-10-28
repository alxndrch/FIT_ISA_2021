.PHONY: all clean zip
CC = g++
CFLAGS = -std=gnu99 -Wall -Wextra -Werror -pedantic

EXECUTABLE = isa

all: $(EXECUTABLE)

isa: $(EXECUTABLE).o
	$(CC) -o $@ $^

isa.o: $(EXECUTABLE).cpp
	$(CC) -c $^

zip:
	zip $(EXECUTABLE).zip *.cpp *.h Makefile *.lua isa.pcap manual.pdf

clean:
	rm -rf $(EXECUTABLE) *.o