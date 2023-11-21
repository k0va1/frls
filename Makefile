.PHONY: frls
frls:
	clang -c cJSON.c -o cJSON.o
	clang -c optparser.c -o optparser.o
	clang -c utils.c -o utils.o
	clang -c transport.c -o transport.o
	clang -c config.c -o config.o
	clang -c commands.c -o commands.o
	clang -Wall -Wextra frls.c cJSON.o optparser.o config.o commands.o utils.o transport.o -o frls
	./frls
