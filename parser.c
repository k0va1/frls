#include "parser.h"
#include "prism/ast.h"
#include "prism/diagnostic.h"
#include "prism/node.h"
#include "utils.h"
#include <stdint.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

int get_line(pm_parser_t *parser, pm_diagnostic_t *error) {
  const uint8_t *source_start = parser->start;
  const uint8_t *error_line_start = error->location.start;
  int line = 1;

  for (uint8_t *c = (uint8_t *)source_start; c < error_line_start; c++) {
    if (*c == '\n') {
      line++;
    }
  }

  return line;
}

ConstHM *build_const_map(char *file_path, pm_parser_t *parser, pm_node_t *node,
                         ConstHM *consts) {
  switch (PM_NODE_TYPE(node)) {
  case PM_PROGRAM_NODE: {
    pm_program_node_t *cast = (pm_program_node_t *)node;
    consts = build_const_map(file_path, parser, (pm_node_t *)cast->statements,
                             consts);
    break;
  }
  case PM_STATEMENTS_NODE: {
    pm_statements_node_t *cast = (pm_statements_node_t *)node;

    size_t last_index = cast->body.size;
    for (uint32_t index = 0; index < last_index; index++) {
      consts = build_const_map(file_path, parser,
                               (pm_node_t *)cast->body.nodes[index], consts);
    }

    break;
  }
  case PM_MODULE_NODE: {
    pm_module_node_t *cast = (pm_module_node_t *)node;
    consts = build_const_map(file_path, parser,
                             (pm_node_t *)cast->constant_path, consts);

    if (cast->body != NULL) {
      consts =
          build_const_map(file_path, parser, (pm_node_t *)cast->body, consts);
    }

    break;
  }
  case PM_TOKEN_KEYWORD_MODULE: {
    pm_module_node_t *cast = (pm_module_node_t *)node;

    // constant_path
    consts = build_const_map(file_path, parser,
                             (pm_node_t *)cast->constant_path, consts);

    // body
    if (cast->body != NULL) {
      consts =
          build_const_map(file_path, parser, (pm_node_t *)cast->body, consts);
    }

    break;
  }
  case PM_CONSTANT_READ_NODE: {
    pm_constant_read_node_t *cast = (pm_constant_read_node_t *)node;
    // TODO: add constants line and column to location struct
    // printf("location: row=%d, col=%d\n", &node->location.start)

    pm_line_column_t start = pm_newline_list_line_column(&parser->newline_list,
                                                         node->location.start);
    Location *l = malloc(sizeof(l));
    l->file_path = strndup(file_path, strlen(file_path));
    pm_constant_t *constant =
        pm_constant_pool_id_to_constant(&parser->constant_pool, cast->name);
    Const *c = malloc(sizeof(c));
    c->locations = l;
    c->const_name = strndup((char *)constant->start, constant->length);
    shput(consts, c->const_name, c);
    break;
  }
  case PM_CONSTANT_PATH_NODE: {

    pm_constant_path_node_t *cast = (pm_constant_path_node_t *)node;
    if (cast->parent != NULL) {
      consts =
          build_const_map(file_path, parser, (pm_node_t *)cast->parent, consts);
    }

    // child
    if (cast->child != NULL) {
      consts =
          build_const_map(file_path, parser, (pm_node_t *)cast->child, consts);
    }
    break;
  }
  case PM_CLASS_NODE: {
    pm_class_node_t *cast = (pm_class_node_t *)node;
    // constant_path
    consts = build_const_map(file_path, parser,
                             (pm_node_t *)cast->constant_path, consts);

    // superclass
    if (cast->superclass != NULL) {
      consts = build_const_map(file_path, parser, (pm_node_t *)cast->superclass,
                               consts);
    }

    // body
    if (cast->body != NULL) {
      consts =
          build_const_map(file_path, parser, (pm_node_t *)cast->body, consts);
    }
  }
  default: {
    break;
  }
  }
  return consts;
}

void print_errors(pm_parser_t *parser) {
  pm_diagnostic_t *error = (pm_diagnostic_t *)parser->error_list.head;
  while (error != NULL) {
    int line = get_line(parser, error);
    printf("Error: %s on line %d\n", error->message, line);
    printf("Location start: %s\n", error->location.start);
    printf("Location end: %s\n", error->location.end);
    error = (pm_diagnostic_t *)error->node.next;
  }
}

void parse(char *file_path, ParsedInfo *parsed_info) {
  char *source = readall(file_path);
  size_t length = strlen(source);

  pm_parser_t parser;
  pm_parser_init(&parser, source, length, NULL);

  pm_node_t *root = pm_parse(&parser);
  if (root != NULL) {
    print_errors(&parser);

    ConstHM *consts = NULL;
    consts = build_const_map(file_path, &parser, root, consts);

    if (consts != NULL) {
      log_info("Const Map built, len: %zu", hmlen(consts));

      if (parsed_info->consts == NULL) {
        parsed_info->consts = consts;
      } else {
        // TODO: merge constants
      }
    }
  } else {
    // prism API doesn't support returning parse errors
    log_info("%d", parser.error_list.head);
  }

  pm_node_destroy(&parser, root);
  pm_parser_free(&parser);
}
