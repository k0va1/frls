#include "ignore.h"
#include "utils.h"
#include <stdio.h>

bool is_ignored_file(char *file_path) {
  return is_includes(IGNORE_FILES, file_name(file_path));
}

bool is_ignored_dir(char *file_path) {
  return is_includes(IGNORE_DIRECTORIES, file_name(file_path));
}
