CC = clang
CFLAGS =-Wall -Wextra
INCLUDES =-I"prism/include"
LIBS=-L"prism/build"

.PHONY: frls test main

frls: build prism_static
	$(CC) $(CFLAGS) $(INCLUDES) $(LIBS) frls.c cJSON.o optparser.o config.o commands.o utils.o transport.o server.o parser.o source.o ignore.o -lprism -o frls
	./frls

build:
	$(CC) $(CFLAGS) -c cJSON.c -o cJSON.o
	$(CC) $(CFLAGS) -c optparser.c -o optparser.o
	$(CC) $(CFLAGS) -c utils.c -o utils.o
	$(CC) $(CFLAGS) $(INCLUDES) -c transport.c -o transport.o
	$(CC) $(CFLAGS) -c config.c -o config.o
	$(CC) $(CFLAGS) $(INCLUDES) -c commands.c -o commands.o
	$(CC) $(CFLAGS) $(INCLUDES) -c server.c -o server.o
	$(CC) $(CFLAGS) $(INCLUDES) -c ignore.c -o ignore.o
	$(CC) $(CFLAGS) $(INCLUDES) -c parser.c -o parser.o
	$(CC) $(CFLAGS) $(INCLUDES) -c source.c -o source.o

prism_static:
	cd prism && $(MAKE) static

test: build
	$(CC) $(CFLAGS) $(INCLUDES) $(LIBS) test/suite.c cJSON.o optparser.o config.o commands.o utils.o transport.o server.o parser.o source.o ignore.o -lprism -o test/suite
	./test/suite

# is needed for experiments
main:
	$(CC) $(CFLAGS) $(INCLUDES) $(LIBS) main.c parser.o source.o utils.o -lprism -o main
	./main
