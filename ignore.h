#include "stdbool.h"

#ifndef IGNORE_H_INCLUDED
#define IGNORE_H_INCLUDED

static const char *IGNORE_FILES[] = {".gitignore"};
static const char *IGNORE_DIRECTORIES[] = {".git"};

bool is_ignored_file(char *file_path);
bool is_ignored_dir(char *file_path);

#endif
