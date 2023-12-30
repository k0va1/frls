#include "optparser.h"
#include "stdbool.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "utils.h"

const char *unary_args[] = {"--version", "-v", "--help", "-h"};
const char *supported_commands[] = {"--version", "-v", "--help", "-h"};
const char *supported_options[] = {"--host", "--port"};

bool is_unary_arg(char *arg) {
  for (size_t i = 0; i < sizeof(unary_args) / sizeof(unary_args[0]); i++) {
    if (strcmp(unary_args[i], arg) == 0) {
      return true;
    }
  }

  return false;
}

ArgKV *parse_options(int argc, char *argv[], size_t *length) {
  if (argc == 1) {
    return NULL;
  }

  *length = argc - 1;
  ArgKV *arg_kvs = malloc(sizeof(ArgKV) * *length);
  for (int i = 1; i < argc; i++) {
    char *kv = argv[i];
    char *key = strtok(kv, "=");
    if (key == NULL) {
      fprintf(stderr,
              "Invalid arguments: provide arguments in `--key=value` format\n");
      exit(1);
    }

    key = remove_leading_dashes(key);
    size_t key_len = strlen(key);
    arg_kvs[i - 1].key = (char *)malloc(sizeof(char *) * key_len + 1);
    strncpy(arg_kvs[i - 1].key, key, key_len);
    arg_kvs[i - 1].key[key_len] = '\0';

    char *value = strtok(NULL, "=");
    if (value == NULL) {
      fprintf(stderr, "Invalid arguments. Value is empty: provide arguments in "
                      "`--key=value` format\n");
      exit(1);
    }
    size_t value_len = strlen(value);
    arg_kvs[i - 1].value = (char *)malloc(sizeof(char *) * value_len + 1);
    strncpy(arg_kvs[i - 1].value, value, value_len);
    arg_kvs[i - 1].value[value_len] = '\0';
  }

  return arg_kvs;
}
