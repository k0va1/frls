CC = clang
CFLAGS =-Wall -Wextra
INCLUDES =-I"prism/include"
LIBS=-L"prism/build"

.PHONY: frls

frls: prism_static
	$(CC) $(CFLAGS) -c cJSON.c -o cJSON.o
	$(CC) $(CFLAGS) -c optparser.c -o optparser.o
	$(CC) $(CFLAGS) -c utils.c -o utils.o
	$(CC) $(CFLAGS) $(INCLUDES) -c transport.c -o transport.o
	$(CC) $(CFLAGS) -c config.c -o config.o
	$(CC) $(CFLAGS) $(INCLUDES) -c commands.c -o commands.o
	$(CC) $(CFLAGS) $(INCLUDES) -c server.c -o server.o
	$(CC) $(CFLAGS) $(INCLUDES) -c parser.c -o parser.o
	$(CC) $(CFLAGS) $(INCLUDES) $(LIBS) frls.c cJSON.o optparser.o config.o commands.o utils.o transport.o server.o parser.o -lprism -o frls
	./frls

prism_static:
	cd prism && $(MAKE) static
