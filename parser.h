#include "prism.h"

#ifndef PARSER_H_INCLUDED
#define PARSER_H_INCLUDED

typedef struct {
  char *file_path;
  size_t row;
  size_t col;
} Location;

typedef struct {
  char *const_name;
  Location *locations;
} Const;

typedef struct {
  char *key;
  Const *value;
} ConstHM;

typedef struct {
  ConstHM *consts;
} ParsedInfo;

void parse(char *file_path, ParsedInfo *parsed_info);

#endif
