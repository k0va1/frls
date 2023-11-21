#include "stdlib.h"

#ifndef OPTPARSER_H_INCLUDED
#define OPTPARSER_H_INCLUDED

typedef struct {
  char *key;
  char *value;
} ArgKV;

ArgKV *parse_options(int argc, char *argv[], size_t *length);

#endif
