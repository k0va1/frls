#include "prism.h"
#include "source.h"

#ifndef PARSER_H_INCLUDED
#define PARSER_H_INCLUDED

// zero-based
typedef struct {
  size_t line;
  size_t character;
} Position;

typedef struct {
  char *file_path;
  Position *start;
  Position *end;
} Location;

typedef struct {
  Position *start;
  Position *end;
} Range;

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

typedef struct {
  pm_node_t *found_node;
  size_t line;
  size_t character;
} VisitArgs;

void parse(Source *source, ParsedInfo *parsed_info);
void traverse_ast(pm_node_t *node, pm_parser_t *parser,
                  void (*visit)(pm_node_t *, pm_parser_t *, void *), void *arg);
void find_node_by_location(pm_node_t *node, pm_parser_t *parser, void *arg);
bool is_matched_location(pm_node_t *node, pm_parser_t *parser, size_t line, size_t character);
void print_consts(ParsedInfo *parsed_info);

#endif
