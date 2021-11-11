.PHONY: all clean zip
CC = g++

EXECUTABLE = client

all: $(EXECUTABLE)

isa: $(EXECUTABLE).o
	$(CC) -o $@ $^

isa.o: $(EXECUTABLE).cpp
	$(CC) -c $^

zip:
	zip $(EXECUTABLE).zip *.cpp *.h Makefile *.lua isa.pcap manual.pdf

clean:
	rm -rf $(EXECUTABLE) *.o