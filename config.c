#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "optparser.h"
#include "config.h"

Config *create_config(int argc, char *argv[]) {
  Config *config = malloc(sizeof(Config *));
  config->host = HOST;
  config->port = PORT;
  config->is_initialized = false;

  if (argc == 1) {
    return config;
  } else {
    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
      printf("%s\n", FRLS_VERSION);
      exit(0);
    } else if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
      printf("%s\n", HELP_MESSAGE);
      exit(0);
    }

    size_t length;
    ArgKV *args = parse_options(argc, argv, &length);
    ArgKV *ptr = args;
    ArgKV *end = args + length;

    while (ptr < end) {
      if (strcmp(ptr->key, "host") == 0) {
        config->host = malloc(sizeof(char) * strlen(ptr->value) + 1);
        strncpy(config->host, ptr->value, strlen(ptr->value));
        config->host[strlen(ptr->value)] = '\0';
      } else if (strcmp(ptr->key, "port") == 0) {
        config->port = atoi(ptr->value);
      }
      free(ptr->key);
      free(ptr->value);

      ptr++;
    }
    free(args);
  }

  return config;
}
