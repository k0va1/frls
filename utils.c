#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>
#include "util.h"

char *trim(char *c) {
  char *e = c + strlen(c) - 1;
  while (*c && isspace(*c))
    c++;
  while (e > c && isspace(*e))
    *e-- = '\0';
  return c;
}

void fail(char *msg) {
  fprintf(stderr, "[ERROR] %s: %s", msg, strerror(errno));
  exit(-1);
}

void info(char *msg, ...) {
  va_list args;
  printf("[INFO] ");
  va_start(args, msg);
  vprintf(msg, args);
  printf("\n");
  va_end(args);
}

void error(char *msg, ...) {
  va_list args;
  printf("[ERROR] ");
  va_start(args, msg);
  vprintf(msg, args);
  printf("\n");
  va_end(args);
}
