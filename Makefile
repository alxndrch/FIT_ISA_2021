.PHONY all clean zip
CC=gcc 
CFLAGS=-std=c11 -Wall -Wextra -Werror -pedantic

EXECUTABLE=isa

all: $(EXECUTABLE)

isa: $(EXECUTABLE).o
	$(CC) -o $@ $^

isa.o: $(EXECUTABLE).c
	$(CC) -c $^

zip:
	zip $(EXECUTABLE).zip *.c *.h Makefile *.lua isa.pcap manual.pdf

clean:
	rm -rf $(EXECUTABLE) *.o
