CC = clang
CFLAGS = -Wall -Wextra
INCLUDES = -I"include" -I"vendor" -I"vendor/cJSON" -I"vendor/prism/include"
LIBS = -L"vendor/prism/build"

BUILD_DIR = build
OBJS = $(BUILD_DIR)/cJSON.o $(BUILD_DIR)/optparser.o $(BUILD_DIR)/config.o $(BUILD_DIR)/commands.o \
       $(BUILD_DIR)/utils.o $(BUILD_DIR)/transport.o $(BUILD_DIR)/server.o $(BUILD_DIR)/parser.o \
       $(BUILD_DIR)/source.o $(BUILD_DIR)/ignore.o

.PHONY: start test main clean all update-prism update-cjson update-stb update-deps

all: frls

frls: $(BUILD_DIR) $(OBJS) prism_static
	$(CC) $(CFLAGS) $(INCLUDES) $(LIBS) src/frls.c $(OBJS) -lprism -o $(BUILD_DIR)/frls

start: frls
	$(BUILD_DIR)/frls

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/cJSON.o: vendor/cJSON/cJSON.c vendor/cJSON/cJSON.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c vendor/cJSON/cJSON.c -o $@

$(BUILD_DIR)/optparser.o: src/optparser.c include/optparser.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c src/optparser.c -o $@

$(BUILD_DIR)/utils.o: src/utils.c include/utils.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c src/utils.c -o $@

$(BUILD_DIR)/transport.o: src/transport.c include/transport.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c src/transport.c -o $@

$(BUILD_DIR)/config.o: src/config.c include/config.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c src/config.c -o $@

$(BUILD_DIR)/commands.o: src/commands.c include/commands.h prism_static | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c src/commands.c -o $@

$(BUILD_DIR)/server.o: src/server.c include/server.h prism_static | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c src/server.c -o $@

$(BUILD_DIR)/ignore.o: src/ignore.c include/ignore.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c src/ignore.c -o $@

$(BUILD_DIR)/parser.o: src/parser.c include/parser.h prism_static | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c src/parser.c -o $@

$(BUILD_DIR)/source.o: src/source.c include/source.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c src/source.c -o $@

prism_static:
	cd vendor/prism && $(MAKE) static

update-prism:
	git submodule update --remote vendor/prism
	cd vendor/prism && $(MAKE) clean && $(MAKE) static

update-cjson:
	git submodule update --remote vendor/cJSON

update-stb:
	curl -o vendor/stb_ds.h https://raw.githubusercontent.com/nothings/stb/master/stb_ds.h

update-deps: update-prism update-cjson update-stb

test: frls
	rake test

# is needed for experiments
main: $(BUILD_DIR) $(BUILD_DIR)/parser.o $(BUILD_DIR)/source.o $(BUILD_DIR)/utils.o prism_static
	$(CC) $(CFLAGS) $(INCLUDES) $(LIBS) src/main.c $(BUILD_DIR)/parser.o $(BUILD_DIR)/source.o $(BUILD_DIR)/utils.o -lprism -o $(BUILD_DIR)/main

clean:
	rm -rf $(BUILD_DIR)
