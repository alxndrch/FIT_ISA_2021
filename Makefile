.PHONY: all clean zip
CC = g++

EXECUTABLE = client

all: $(EXECUTABLE)

client:
	$(CC) -o $@ isa.cpp
 
zip:
	zip $(EXECUTABLE).zip *.cpp *.h Makefile *.lua isa.pcap manual.pdf

clean:
	rm -rf $(EXECUTABLE) *.o