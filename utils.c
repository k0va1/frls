#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdbool.h>

#include "utils.h"

char *trim(char *c) {
  char *e = c + strlen(c) - 1;
  while (*c && isspace(*c))
    c++;
  while (e > c && isspace(*e))
    *e-- = '\0';
  return c;
}

bool is_ends_with(char *left, char *right) {
  if (left == NULL || right == NULL)
    return false;

  for (int ri = strlen(right), li = strlen(left); ri >= 0; ri--, li--) {
    if (right[ri] != left[li])
      return false;
  }
  return true;
}

bool is_starts_with(char *left, char *right) {
  if (left == NULL || right == NULL)
    return false;

  for (size_t ri = 0, li = 0; ri < strlen(right); ri++, li++) {
    if (right[ri] != left[li])
      return false;
  }
  return true;
}

char *delete_prefix(char *str, char *prefix) {
  assert(prefix != 0 && str != 0);

  if (is_starts_with(str, prefix)) {
    return str + strlen(prefix);
  } else {
    return str;
  }
}

char *remove_leading_dashes(char *arg) {
  size_t dash_count = 0;
  char *c = arg;

  while (*c == '-') {
    dash_count++;
    c++;
  }

  return arg + dash_count;
}

void fail(char *msg) {
  fprintf(stderr, "[ERROR] %s: %s", msg, strerror(errno));
  exit(-1);
}

void log_info(char *msg, ...) {
  va_list args;
  printf("[INFO] ");
  va_start(args, msg);
  vprintf(msg, args);
  printf("\n");
  va_end(args);
}

void log_error(char *msg, ...) {
  va_list args;
  printf("[ERROR] ");
  va_start(args, msg);
  vprintf(msg, args);
  printf("\n");
  va_end(args);
}

// File utils
bool is_dir(char *file_path) {
  int fd = open(file_path, O_RDONLY, 0);
  if (fd == -1)
    return -1;

  struct stat stat_buf;
  if (fstat(fd, &stat_buf) == -1)
    return -1;

  return (stat_buf.st_mode & S_IFMT) == S_IFDIR ? 1 : 0;
}

bool is_file(char *file_path) {
  int fd = open(file_path, O_RDONLY, 0);
  if (fd == -1)
    return -1;

  struct stat stat_buf;
  if (fstat(fd, &stat_buf) == -1)
    return -1;

  return (stat_buf.st_mode & S_IFMT) == S_IFREG ? 1 : 0;
}

char *get_file_path(char *uri) {
  assert(uri != NULL);
  return delete_prefix(uri, "file://");
}

char *readall(char *file_path) {
  FILE *f = fopen(file_path, "rb");
  fseek(f, 0, SEEK_END);
  long fsize = ftell(f);
  fseek(f, 0, SEEK_SET); /* same as rewind(f); */

  char *content = malloc(fsize + 1);
  fread(content, fsize, 1, f);
  fclose(f);

  content[fsize] = 0;
  return content;
}
