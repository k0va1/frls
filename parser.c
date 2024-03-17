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

// TODO: use traverse_ast for traversing
ConstHM *build_const_map(char *file_path, pm_parser_t *parser, pm_node_t *node, ConstHM *consts) {
  switch (PM_NODE_TYPE(node)) {
  case PM_PROGRAM_NODE: {
    pm_program_node_t *cast = (pm_program_node_t *)node;
    consts = build_const_map(file_path, parser, (pm_node_t *)cast->statements, consts);
    break;
  }
  case PM_STATEMENTS_NODE: {
    pm_statements_node_t *cast = (pm_statements_node_t *)node;

    size_t last_index = cast->body.size;
    for (uint32_t index = 0; index < last_index; index++) {
      consts = build_const_map(file_path, parser, (pm_node_t *)cast->body.nodes[index], consts);
    }

    break;
  }
  case PM_MODULE_NODE: {
    pm_module_node_t *cast = (pm_module_node_t *)node;
    consts = build_const_map(file_path, parser, (pm_node_t *)cast->constant_path, consts);

    if (cast->body != NULL) {
      consts = build_const_map(file_path, parser, (pm_node_t *)cast->body, consts);
    }

    break;
  }
  case PM_TOKEN_KEYWORD_MODULE: {
    pm_module_node_t *cast = (pm_module_node_t *)node;

    // constant_path
    consts = build_const_map(file_path, parser, (pm_node_t *)cast->constant_path, consts);

    // body
    if (cast->body != NULL) {
      consts = build_const_map(file_path, parser, (pm_node_t *)cast->body, consts);
    }

    break;
  }
  case PM_CONSTANT_READ_NODE: {
    pm_constant_read_node_t *cast = (pm_constant_read_node_t *)node;
    pm_line_column_t start = pm_newline_list_line_column(&parser->newline_list,
                                                         node->location.start, parser->start_line);
    pm_line_column_t end =
        pm_newline_list_line_column(&parser->newline_list, node->location.end, parser->start_line);

    Position *start_pos = malloc(sizeof(Position));
    start_pos->line = start.line - 1;
    start_pos->character = start.column;

    Position *end_pos = malloc(sizeof(Position));
    end_pos->line = end.line - 1;
    end_pos->character = end.column;

    Location l = {
        .file_path = strndup(file_path, strlen(file_path)), .start = start_pos, .end = end_pos};
    pm_constant_t *constant = pm_constant_pool_id_to_constant(&parser->constant_pool, cast->name);
    Const *c = malloc(sizeof(Const));
    c->const_name = strndup((char *)constant->start, constant->length);
    arrpush(c->locations, l);
    shput(consts, c->const_name, c);
    break;
  }
  case PM_CONSTANT_PATH_NODE: {
    pm_constant_path_node_t *cast = (pm_constant_path_node_t *)node;
    if (cast->parent != NULL) {
      consts = build_const_map(file_path, parser, (pm_node_t *)cast->parent, consts);
    }

    // child
    if (cast->child != NULL) {
      consts = build_const_map(file_path, parser, (pm_node_t *)cast->child, consts);
    }
    break;
  }
  case PM_CLASS_NODE: {
    pm_class_node_t *cast = (pm_class_node_t *)node;
    // constant_path
    consts = build_const_map(file_path, parser, (pm_node_t *)cast->constant_path, consts);

    // superclass
    if (cast->superclass != NULL) {
      consts = build_const_map(file_path, parser, (pm_node_t *)cast->superclass, consts);
    }

    // body
    if (cast->body != NULL) {
      consts = build_const_map(file_path, parser, (pm_node_t *)cast->body, consts);
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

bool is_equals_locations(Location *l1, Location *l2) {
  return strcmp(l1->file_path, l2->file_path) == 0 && l1->start->line == l2->start->line &&
         l1->start->character == l2->start->character && l1->end->line == l2->end->line &&
         l1->end->character == l2->end->character;
}

void parse(Source *source, ParsedInfo *parsed_info) {
  assert(source != NULL);

  if (source->parser != NULL) {
    pm_parser_free(source->parser);
  }

  if (source->root != NULL) {
    free(source->root);
  }

  pm_parser_t *parser = malloc(sizeof(pm_parser_t));
  size_t src_len = strlen(source->content);
  const uint8_t *src = (const uint8_t *)strndup(source->content, src_len);
  pm_parser_init(parser, src, src_len, NULL);

  pm_node_t *root = pm_parse(parser);
  if (root != NULL) {
    source->root = root;
    source->parser = parser;
    print_errors(parser);

    ConstHM *source_consts = NULL;
    source_consts = build_const_map(source->file_path, parser, root, source_consts);

    if (source_consts != NULL) {
      for (int i = 0; i < hmlen(source_consts); i++) {
        if (shgeti(parsed_info->consts, source_consts[i].key) >= 0) {
          Const *parsed_const = shget(parsed_info->consts, source_consts[i].key);

          for (int k = 0; k < arrlen(source_consts[i].value->locations); k++) {
            Location source_location = source_consts[i].value->locations[k];

            bool has_location = false;
            for (int j = 0; j < arrlen(parsed_const->locations); j++) {
              Location parsed_location = parsed_const->locations[j];

              if (is_equals_locations(&parsed_location, &source_location)) {
                has_location = true;
                break;
              }
            }

            if (!has_location) {
              arrpush(parsed_const->locations, source_location);
            }
          }
        } else {
          shputi(parsed_info->consts, source_consts[i].key, source_consts[i].value);
        }
      }
    }
  } else {
    // prism API doesn't support returning parse errors
    log_info("%d", parser->error_list.head);
  }
}

bool is_matched_location(pm_node_t *node, pm_parser_t *parser, size_t line, size_t character) {
  pm_line_column_t start =
      pm_newline_list_line_column(&parser->newline_list, node->location.start, parser->start_line);
  pm_line_column_t end =
      pm_newline_list_line_column(&parser->newline_list, node->location.end, parser->start_line);

  return (line == start.line && line == end.line) &&
         (character >= start.column && character <= end.column);
}

void traverse_ast(pm_node_t *node, pm_parser_t *parser,
                  void (*visit)(pm_node_t *, pm_parser_t *, void *), void *arg) {
  if (node == NULL)
    return;

  visit(node, parser, arg);

  switch (PM_NODE_TYPE(node)) {
  case PM_SCOPE_NODE:
    // We do not need to print a ScopeNode as it's not part of the AST.
    return;
  case PM_ALIAS_GLOBAL_VARIABLE_NODE: {
    pm_alias_global_variable_node_t *cast = (pm_alias_global_variable_node_t *)node;

    traverse_ast((pm_node_t *)cast->new_name, parser, visit, arg);
    traverse_ast((pm_node_t *)cast->old_name, parser, visit, arg);

    break;
  }
  case PM_ALIAS_METHOD_NODE: {
    pm_alias_method_node_t *cast = (pm_alias_method_node_t *)node;

    traverse_ast((pm_node_t *)cast->new_name, parser, visit, arg);
    traverse_ast((pm_node_t *)cast->old_name, parser, visit, arg);

    break;
  }
  case PM_ALTERNATION_PATTERN_NODE: {
    pm_alternation_pattern_node_t *cast = (pm_alternation_pattern_node_t *)node;

    traverse_ast((pm_node_t *)cast->left, parser, visit, arg);
    traverse_ast((pm_node_t *)cast->right, parser, visit, arg);

    break;
  }
  case PM_AND_NODE: {
    pm_and_node_t *cast = (pm_and_node_t *)node;

    traverse_ast((pm_node_t *)cast->left, parser, visit, arg);
    traverse_ast((pm_node_t *)cast->right, parser, visit, arg);

    break;
  }
  case PM_ARGUMENTS_NODE: {
    pm_arguments_node_t *cast = (pm_arguments_node_t *)node;

    size_t last_index = cast->arguments.size;
    for (uint32_t index = 0; index < last_index; index++) {
      traverse_ast((pm_node_t *)cast->arguments.nodes[index], parser, visit, arg);
    }

    break;
  }
  case PM_ARRAY_NODE: {
    pm_array_node_t *cast = (pm_array_node_t *)node;

    size_t last_index = cast->elements.size;
    for (uint32_t index = 0; index < last_index; index++) {
      traverse_ast((pm_node_t *)cast->elements.nodes[index], parser, visit, arg);
    }

    break;
  }
  case PM_ARRAY_PATTERN_NODE: {
    pm_array_pattern_node_t *cast = (pm_array_pattern_node_t *)node;

    // constant
    if (cast->constant != NULL) {
      traverse_ast((pm_node_t *)cast->constant, parser, visit, arg);
    }

    size_t last_index = cast->requireds.size;
    for (uint32_t index = 0; index < last_index; index++) {
      traverse_ast((pm_node_t *)cast->requireds.nodes[index], parser, visit, arg);
    }

    if (cast->rest != NULL) {
      traverse_ast((pm_node_t *)cast->rest, parser, visit, arg);
    }

    last_index = cast->posts.size;
    for (uint32_t index = 0; index < last_index; index++) {
      traverse_ast((pm_node_t *)cast->posts.nodes[index], parser, visit, arg);
    }

    break;
  }
  case PM_ASSOC_NODE: {
    pm_assoc_node_t *cast = (pm_assoc_node_t *)node;

    traverse_ast((pm_node_t *)cast->key, parser, visit, arg);
    traverse_ast((pm_node_t *)cast->value, parser, visit, arg);

    break;
  }
  case PM_ASSOC_SPLAT_NODE: {
    pm_assoc_splat_node_t *cast = (pm_assoc_splat_node_t *)node;

    // value
    {
      if (cast->value == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->value, parser, visit, arg);
      }
    }

    break;
  }
  case PM_BACK_REFERENCE_READ_NODE: {
    pm_back_reference_read_node_t *cast = (pm_back_reference_read_node_t *)node;

    break;
  }
  case PM_BEGIN_NODE: {
    pm_begin_node_t *cast = (pm_begin_node_t *)node;

    // statements
    {
      if (cast->statements == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->statements, parser, visit, arg);
      }
    }

    // else_clause
    {
      if (cast->else_clause == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->else_clause, parser, visit, arg);
      }
    }

    break;
  }
  case PM_BLOCK_ARGUMENT_NODE: {
    pm_block_argument_node_t *cast = (pm_block_argument_node_t *)node;

    // expression
    {
      if (cast->expression == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->expression, parser, visit, arg);
      }
    }

    break;
  }
  case PM_BLOCK_LOCAL_VARIABLE_NODE: {
    pm_block_local_variable_node_t *cast = (pm_block_local_variable_node_t *)node;

    break;
  }
  case PM_BLOCK_NODE: {
    pm_block_node_t *cast = (pm_block_node_t *)node;

    // parameters
    {
      if (cast->parameters == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->parameters, parser, visit, arg);
      }
    }

    // body
    {
      if (cast->body == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->body, parser, visit, arg);
      }
    }

    break;
  }
  case PM_BLOCK_PARAMETER_NODE: {
    pm_block_parameter_node_t *cast = (pm_block_parameter_node_t *)node;

    break;
  }
  case PM_BLOCK_PARAMETERS_NODE: {
    pm_block_parameters_node_t *cast = (pm_block_parameters_node_t *)node;

    // parameters
    {
      if (cast->parameters == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->parameters, parser, visit, arg);
      }
    }

    break;
  }
  case PM_BREAK_NODE: {
    pm_break_node_t *cast = (pm_break_node_t *)node;

    // arguments
    {
      if (cast->arguments == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->arguments, parser, visit, arg);
      }
    }

    break;
  }
  case PM_CALL_AND_WRITE_NODE: {
    pm_call_and_write_node_t *cast = (pm_call_and_write_node_t *)node;

    // receiver
    {
      if (cast->receiver == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->receiver, parser, visit, arg);
      }
    }

    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_CALL_NODE: {
    pm_call_node_t *cast = (pm_call_node_t *)node;

    // receiver
    {
      if (cast->receiver == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->receiver, parser, visit, arg);
      }
    }

    // arguments
    {
      if (cast->arguments == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->arguments, parser, visit, arg);
      }
    }

    // block
    {
      if (cast->block == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->block, parser, visit, arg);
      }
    }

    break;
  }
  case PM_CALL_OPERATOR_WRITE_NODE: {
    pm_call_operator_write_node_t *cast = (pm_call_operator_write_node_t *)node;

    // receiver
    {
      if (cast->receiver == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->receiver, parser, visit, arg);
      }
    }

    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_CALL_OR_WRITE_NODE: {
    pm_call_or_write_node_t *cast = (pm_call_or_write_node_t *)node;

    // receiver
    {
      if (cast->receiver == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->receiver, parser, visit, arg);
      }
    }
    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_CALL_TARGET_NODE: {
    pm_call_target_node_t *cast = (pm_call_target_node_t *)node;

    // receiver
    { traverse_ast((pm_node_t *)cast->receiver, parser, visit, arg); }

    break;
  }
  case PM_CAPTURE_PATTERN_NODE: {
    pm_capture_pattern_node_t *cast = (pm_capture_pattern_node_t *)node;

    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    // target
    { traverse_ast((pm_node_t *)cast->target, parser, visit, arg); }

    break;
  }
  case PM_CASE_MATCH_NODE: {
    pm_case_match_node_t *cast = (pm_case_match_node_t *)node;

    // predicate
    {
      if (cast->predicate == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->predicate, parser, visit, arg);
      }
    }

    // consequent
    {
      if (cast->consequent == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->consequent, parser, visit, arg);
      }
    }

    break;
  }
  case PM_CASE_NODE: {
    pm_case_node_t *cast = (pm_case_node_t *)node;

    // predicate
    {
      if (cast->predicate == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->predicate, parser, visit, arg);
      }
    }

    // consequent
    {
      if (cast->consequent == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->consequent, parser, visit, arg);
      }
    }

    break;
  }
  case PM_CLASS_NODE: {
    pm_class_node_t *cast = (pm_class_node_t *)node;

    printf("class\n");
    // constant_path
    { traverse_ast((pm_node_t *)cast->constant_path, parser, visit, arg); }

    // superclass
    {
      if (cast->superclass == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->superclass, parser, visit, arg);
      }
    }

    // body
    {
      if (cast->body == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->body, parser, visit, arg);
      }
    }

    break;
  }
  case PM_CLASS_VARIABLE_AND_WRITE_NODE: {
    pm_class_variable_and_write_node_t *cast = (pm_class_variable_and_write_node_t *)node;

    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_CLASS_VARIABLE_OPERATOR_WRITE_NODE: {
    pm_class_variable_operator_write_node_t *cast = (pm_class_variable_operator_write_node_t *)node;

    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_CLASS_VARIABLE_OR_WRITE_NODE: {
    pm_class_variable_or_write_node_t *cast = (pm_class_variable_or_write_node_t *)node;

    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_CLASS_VARIABLE_READ_NODE: {
    pm_class_variable_read_node_t *cast = (pm_class_variable_read_node_t *)node;

    break;
  }
  case PM_CLASS_VARIABLE_TARGET_NODE: {
    pm_class_variable_target_node_t *cast = (pm_class_variable_target_node_t *)node;

    break;
  }
  case PM_CLASS_VARIABLE_WRITE_NODE: {
    pm_class_variable_write_node_t *cast = (pm_class_variable_write_node_t *)node;

    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_CONSTANT_AND_WRITE_NODE: {
    pm_constant_and_write_node_t *cast = (pm_constant_and_write_node_t *)node;

    printf("class\n");
    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_CONSTANT_OPERATOR_WRITE_NODE: {
    pm_constant_operator_write_node_t *cast = (pm_constant_operator_write_node_t *)node;

    printf("class\n");
    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_CONSTANT_OR_WRITE_NODE: {
    pm_constant_or_write_node_t *cast = (pm_constant_or_write_node_t *)node;

    printf("class\n");
    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_CONSTANT_PATH_AND_WRITE_NODE: {
    pm_constant_path_and_write_node_t *cast = (pm_constant_path_and_write_node_t *)node;

    printf("class\n");
    // target
    { traverse_ast((pm_node_t *)cast->target, parser, visit, arg); }

    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_CONSTANT_PATH_NODE: {
    pm_constant_path_node_t *cast = (pm_constant_path_node_t *)node;

    printf("class\n");
    // parent
    {
      if (cast->parent == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->parent, parser, visit, arg);
      }
    }

    // child
    { traverse_ast((pm_node_t *)cast->child, parser, visit, arg); }

    break;
  }
  case PM_CONSTANT_PATH_OPERATOR_WRITE_NODE: {
    pm_constant_path_operator_write_node_t *cast = (pm_constant_path_operator_write_node_t *)node;

    // target
    { traverse_ast((pm_node_t *)cast->target, parser, visit, arg); }

    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_CONSTANT_PATH_OR_WRITE_NODE: {
    pm_constant_path_or_write_node_t *cast = (pm_constant_path_or_write_node_t *)node;

    // target
    { traverse_ast((pm_node_t *)cast->target, parser, visit, arg); }

    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_CONSTANT_PATH_TARGET_NODE: {
    pm_constant_path_target_node_t *cast = (pm_constant_path_target_node_t *)node;

    // parent
    {
      if (cast->parent == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->parent, parser, visit, arg);
      }
    }

    // child
    { traverse_ast((pm_node_t *)cast->child, parser, visit, arg); }

    break;
  }
  case PM_CONSTANT_PATH_WRITE_NODE: {
    pm_constant_path_write_node_t *cast = (pm_constant_path_write_node_t *)node;

    // target
    { traverse_ast((pm_node_t *)cast->target, parser, visit, arg); }

    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_CONSTANT_READ_NODE: {
    pm_constant_read_node_t *cast = (pm_constant_read_node_t *)node;

    pm_constant_t *constant = pm_constant_pool_id_to_constant(&parser->constant_pool, cast->name);

    printf("const %s\n", strndup(constant->start, constant->length));

    break;
  }
  case PM_CONSTANT_TARGET_NODE: {
    pm_constant_target_node_t *cast = (pm_constant_target_node_t *)node;

    break;
  }
  case PM_CONSTANT_WRITE_NODE: {
    pm_constant_write_node_t *cast = (pm_constant_write_node_t *)node;

    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_DEF_NODE: {
    pm_def_node_t *cast = (pm_def_node_t *)node;

    // receiver
    {
      if (cast->receiver == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->receiver, parser, visit, arg);
      }
    }

    // parameters
    {
      if (cast->parameters == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->parameters, parser, visit, arg);
      }
    }

    // body
    {
      if (cast->body == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->body, parser, visit, arg);
      }
    }

    break;
  }
  case PM_DEFINED_NODE: {
    pm_defined_node_t *cast = (pm_defined_node_t *)node;

    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_ELSE_NODE: {
    pm_else_node_t *cast = (pm_else_node_t *)node;

    // statements
    {
      if (cast->statements == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->statements, parser, visit, arg);
      }
    }

    break;
  }
  case PM_EMBEDDED_STATEMENTS_NODE: {
    pm_embedded_statements_node_t *cast = (pm_embedded_statements_node_t *)node;

    {
      if (cast->statements == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->statements, parser, visit, arg);
      }
    }
  }
  case PM_EMBEDDED_VARIABLE_NODE: {
    pm_embedded_variable_node_t *cast = (pm_embedded_variable_node_t *)node;

    // variable
    { traverse_ast((pm_node_t *)cast->variable, parser, visit, arg); }

    break;
  }
  case PM_ENSURE_NODE: {
    pm_ensure_node_t *cast = (pm_ensure_node_t *)node;

    // statements
    {
      if (cast->statements == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->statements, parser, visit, arg);
      }
    }

    break;
  }
  case PM_FALSE_NODE: {

    break;
  }
  case PM_FIND_PATTERN_NODE: {
    pm_find_pattern_node_t *cast = (pm_find_pattern_node_t *)node;

    // constant
    {
      if (cast->constant == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->constant, parser, visit, arg);
      }
    }

    // left
    { traverse_ast((pm_node_t *)cast->left, parser, visit, arg); }

    // right
    { traverse_ast((pm_node_t *)cast->right, parser, visit, arg); }

    break;
  }
  case PM_FLIP_FLOP_NODE: {
    pm_flip_flop_node_t *cast = (pm_flip_flop_node_t *)node;

    // left
    {
      if (cast->left == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->left, parser, visit, arg);
      }
    }

    // right
    {
      if (cast->right == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->right, parser, visit, arg);
      }
    }

    break;
  }
  case PM_FLOAT_NODE: {
    pm_float_node_t *cast = (pm_float_node_t *)node;

    break;
  }
  case PM_FOR_NODE: {
    pm_for_node_t *cast = (pm_for_node_t *)node;

    // index
    { traverse_ast((pm_node_t *)cast->index, parser, visit, arg); }

    // collection
    { traverse_ast((pm_node_t *)cast->collection, parser, visit, arg); }

    // statements
    {
      if (cast->statements == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->statements, parser, visit, arg);
      }
    }

    break;
  }
  case PM_FORWARDING_ARGUMENTS_NODE: {

    break;
  }
  case PM_FORWARDING_PARAMETER_NODE: {

    break;
  }
  case PM_FORWARDING_SUPER_NODE: {
    pm_forwarding_super_node_t *cast = (pm_forwarding_super_node_t *)node;

    // block
    {
      if (cast->block == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->block, parser, visit, arg);
      }
    }

    break;
  }
  case PM_GLOBAL_VARIABLE_AND_WRITE_NODE: {
    pm_global_variable_and_write_node_t *cast = (pm_global_variable_and_write_node_t *)node;

    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_GLOBAL_VARIABLE_OPERATOR_WRITE_NODE: {
    pm_global_variable_operator_write_node_t *cast =
        (pm_global_variable_operator_write_node_t *)node;

    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_GLOBAL_VARIABLE_OR_WRITE_NODE: {
    pm_global_variable_or_write_node_t *cast = (pm_global_variable_or_write_node_t *)node;

    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_GLOBAL_VARIABLE_READ_NODE: {
    pm_global_variable_read_node_t *cast = (pm_global_variable_read_node_t *)node;

    break;
  }
  case PM_GLOBAL_VARIABLE_TARGET_NODE: {
    pm_global_variable_target_node_t *cast = (pm_global_variable_target_node_t *)node;

    break;
  }
  case PM_GLOBAL_VARIABLE_WRITE_NODE: {
    pm_global_variable_write_node_t *cast = (pm_global_variable_write_node_t *)node;

    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_HASH_NODE: {
    pm_hash_node_t *cast = (pm_hash_node_t *)node;
  }
  case PM_HASH_PATTERN_NODE: {
    pm_hash_pattern_node_t *cast = (pm_hash_pattern_node_t *)node;

    // constant
    {
      if (cast->constant == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->constant, parser, visit, arg);
      }
    }

    // rest
    {
      if (cast->rest == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->rest, parser, visit, arg);
      }
    }

    break;
  }
  case PM_IF_NODE: {
    pm_if_node_t *cast = (pm_if_node_t *)node;

    // predicate
    { traverse_ast((pm_node_t *)cast->predicate, parser, visit, arg); }

    // statements
    {
      if (cast->statements == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->statements, parser, visit, arg);
      }
    }

    // consequent
    {
      if (cast->consequent == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->consequent, parser, visit, arg);
      }
    }

    break;
  }
  case PM_IMAGINARY_NODE: {
    pm_imaginary_node_t *cast = (pm_imaginary_node_t *)node;

    // numeric
    { traverse_ast((pm_node_t *)cast->numeric, parser, visit, arg); }

    break;
  }
  case PM_IMPLICIT_NODE: {
    pm_implicit_node_t *cast = (pm_implicit_node_t *)node;

    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_IMPLICIT_REST_NODE: {

    break;
  }
  case PM_IN_NODE: {
    pm_in_node_t *cast = (pm_in_node_t *)node;

    // pattern
    { traverse_ast((pm_node_t *)cast->pattern, parser, visit, arg); }

    // statements
    {
      if (cast->statements == NULL) {
      } else {
        traverse_ast((pm_node_t *)cast->statements, parser, visit, arg);
      }
    }

    break;
  }
  case PM_INDEX_AND_WRITE_NODE: {
    pm_index_and_write_node_t *cast = (pm_index_and_write_node_t *)node;

    // receiver
    {
      if (cast->receiver == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->receiver, parser, visit, arg);
      }
    }

    {
      if (cast->arguments == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->arguments, parser, visit, arg);
      }
    }

    {
      if (cast->block == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->block, parser, visit, arg);
      }
    }

    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_INDEX_OPERATOR_WRITE_NODE: {
    pm_index_operator_write_node_t *cast = (pm_index_operator_write_node_t *)node;

    // receiver
    {
      if (cast->receiver == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->receiver, parser, visit, arg);
      }
    }

    {
      if (cast->arguments == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->arguments, parser, visit, arg);
      }
    }

    {
      if (cast->block == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->block, parser, visit, arg);
      }
    }

    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_INDEX_OR_WRITE_NODE: {
    pm_index_or_write_node_t *cast = (pm_index_or_write_node_t *)node;

    // receiver
    {
      if (cast->receiver == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->receiver, parser, visit, arg);
      }
    }

    {
      if (cast->arguments == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->arguments, parser, visit, arg);
      }
    }

    {
      if (cast->block == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->block, parser, visit, arg);
      }
    }

    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_INDEX_TARGET_NODE: {
    pm_index_target_node_t *cast = (pm_index_target_node_t *)node;

    // receiver
    { traverse_ast((pm_node_t *)cast->receiver, parser, visit, arg); }

    {
      if (cast->arguments == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->arguments, parser, visit, arg);
      }
    }

    {
      if (cast->block == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->block, parser, visit, arg);
      }
    }

    break;
  }
  case PM_INSTANCE_VARIABLE_AND_WRITE_NODE: {
    pm_instance_variable_and_write_node_t *cast = (pm_instance_variable_and_write_node_t *)node;

    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_INSTANCE_VARIABLE_OPERATOR_WRITE_NODE: {
    pm_instance_variable_operator_write_node_t *cast =
        (pm_instance_variable_operator_write_node_t *)node;

    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_INSTANCE_VARIABLE_OR_WRITE_NODE: {
    pm_instance_variable_or_write_node_t *cast = (pm_instance_variable_or_write_node_t *)node;

    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_INSTANCE_VARIABLE_READ_NODE: {
    pm_instance_variable_read_node_t *cast = (pm_instance_variable_read_node_t *)node;

    pm_constant_t *constant = pm_constant_pool_id_to_constant(&parser->constant_pool, cast->name);

    printf("instance var %s\n", strndup((char *)constant->start, constant->length));
    break;
  }
  case PM_INSTANCE_VARIABLE_TARGET_NODE: {
    pm_instance_variable_target_node_t *cast = (pm_instance_variable_target_node_t *)node;

    break;
  }
  case PM_INSTANCE_VARIABLE_WRITE_NODE: {
    pm_instance_variable_write_node_t *cast = (pm_instance_variable_write_node_t *)node;

    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_INTEGER_NODE: {
    pm_integer_node_t *cast = (pm_integer_node_t *)node;

    break;
  }
  case PM_INTERPOLATED_MATCH_LAST_LINE_NODE: {
    pm_interpolated_match_last_line_node_t *cast = (pm_interpolated_match_last_line_node_t *)node;
  }
  case PM_INTERPOLATED_REGULAR_EXPRESSION_NODE: {
    pm_interpolated_regular_expression_node_t *cast =
        (pm_interpolated_regular_expression_node_t *)node;
  }
  case PM_INTERPOLATED_STRING_NODE: {
    pm_interpolated_string_node_t *cast = (pm_interpolated_string_node_t *)node;

    break;
  }
  case PM_INTERPOLATED_SYMBOL_NODE: {
    pm_interpolated_symbol_node_t *cast = (pm_interpolated_symbol_node_t *)node;
    printf("symbol node\n");

    break;
  }
  case PM_INTERPOLATED_X_STRING_NODE: {
    pm_interpolated_x_string_node_t *cast = (pm_interpolated_x_string_node_t *)node;

    break;
  }
  case PM_IT_PARAMETERS_NODE: {

    break;
  }
  case PM_KEYWORD_HASH_NODE: {
    pm_keyword_hash_node_t *cast = (pm_keyword_hash_node_t *)node;

    break;
  }
  case PM_KEYWORD_REST_PARAMETER_NODE: {
    pm_keyword_rest_parameter_node_t *cast = (pm_keyword_rest_parameter_node_t *)node;

    break;
  }
  case PM_LAMBDA_NODE: {
    pm_lambda_node_t *cast = (pm_lambda_node_t *)node;

    // parameters
    {
      if (cast->parameters == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->parameters, parser, visit, arg);
      }
    }

    // body
    {
      if (cast->body == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->body, parser, visit, arg);
      }
    }

    break;
  }
  case PM_LOCAL_VARIABLE_AND_WRITE_NODE: {
    pm_local_variable_and_write_node_t *cast = (pm_local_variable_and_write_node_t *)node;

    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_LOCAL_VARIABLE_OPERATOR_WRITE_NODE: {
    pm_local_variable_operator_write_node_t *cast = (pm_local_variable_operator_write_node_t *)node;

    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_LOCAL_VARIABLE_OR_WRITE_NODE: {
    pm_local_variable_or_write_node_t *cast = (pm_local_variable_or_write_node_t *)node;

    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_LOCAL_VARIABLE_READ_NODE: {
    pm_local_variable_read_node_t *cast = (pm_local_variable_read_node_t *)node;

    pm_constant_t *constant = pm_constant_pool_id_to_constant(&parser->constant_pool, cast->name);

    printf("local var %s\n", strndup((char *)constant->start, constant->length));

    break;
  }
  case PM_LOCAL_VARIABLE_TARGET_NODE: {
    pm_local_variable_target_node_t *cast = (pm_local_variable_target_node_t *)node;

    break;
  }
  case PM_LOCAL_VARIABLE_WRITE_NODE: {
    pm_local_variable_write_node_t *cast = (pm_local_variable_write_node_t *)node;

    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_MATCH_LAST_LINE_NODE: {
    pm_match_last_line_node_t *cast = (pm_match_last_line_node_t *)node;

    break;
  }
  case PM_MATCH_PREDICATE_NODE: {
    pm_match_predicate_node_t *cast = (pm_match_predicate_node_t *)node;

    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    // pattern
    { traverse_ast((pm_node_t *)cast->pattern, parser, visit, arg); }

    break;
  }
  case PM_MATCH_REQUIRED_NODE: {
    pm_match_required_node_t *cast = (pm_match_required_node_t *)node;

    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    // pattern
    { traverse_ast((pm_node_t *)cast->pattern, parser, visit, arg); }

    break;
  }
  case PM_MATCH_WRITE_NODE: {
    pm_match_write_node_t *cast = (pm_match_write_node_t *)node;

    // call
    { traverse_ast((pm_node_t *)cast->call, parser, visit, arg); }

    break;
  }
  case PM_MISSING_NODE: {

    break;
  }
  case PM_MODULE_NODE: {
    pm_module_node_t *cast = (pm_module_node_t *)node;
    printf("module\n");

    // constant_path
    { traverse_ast((pm_node_t *)cast->constant_path, parser, visit, arg); }

    // body
    {
      if (cast->body == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->body, parser, visit, arg);
      }
    }

    break;
  }
  case PM_MULTI_TARGET_NODE: {
    pm_multi_target_node_t *cast = (pm_multi_target_node_t *)node;

    // rest
    {
      if (cast->rest == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->rest, parser, visit, arg);
      }
    }

    break;
  }
  case PM_MULTI_WRITE_NODE: {
    pm_multi_write_node_t *cast = (pm_multi_write_node_t *)node;

    // rest
    {
      if (cast->rest == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->rest, parser, visit, arg);
      }
    }

    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_NEXT_NODE: {
    pm_next_node_t *cast = (pm_next_node_t *)node;

    // arguments
    {
      if (cast->arguments == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->arguments, parser, visit, arg);
      }
    }

    break;
  }
  case PM_NIL_NODE: {

    break;
  }
  case PM_NO_KEYWORDS_PARAMETER_NODE: {
    pm_no_keywords_parameter_node_t *cast = (pm_no_keywords_parameter_node_t *)node;

    break;
  }
  case PM_NUMBERED_PARAMETERS_NODE: {
    pm_numbered_parameters_node_t *cast = (pm_numbered_parameters_node_t *)node;

    break;
  }
  case PM_NUMBERED_REFERENCE_READ_NODE: {
    pm_numbered_reference_read_node_t *cast = (pm_numbered_reference_read_node_t *)node;

    break;
  }
  case PM_OPTIONAL_KEYWORD_PARAMETER_NODE: {
    pm_optional_keyword_parameter_node_t *cast = (pm_optional_keyword_parameter_node_t *)node;

    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_OPTIONAL_PARAMETER_NODE: {
    pm_optional_parameter_node_t *cast = (pm_optional_parameter_node_t *)node;

    // value
    { traverse_ast((pm_node_t *)cast->value, parser, visit, arg); }

    break;
  }
  case PM_OR_NODE: {
    pm_or_node_t *cast = (pm_or_node_t *)node;

    // left
    { traverse_ast((pm_node_t *)cast->left, parser, visit, arg); }

    // right
    { traverse_ast((pm_node_t *)cast->right, parser, visit, arg); }

    break;
  }
  case PM_PARAMETERS_NODE: {
    pm_parameters_node_t *cast = (pm_parameters_node_t *)node;

    // rest
    {
      if (cast->rest == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->rest, parser, visit, arg);
      }
    }

    // keyword_rest
    {
      if (cast->keyword_rest == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->keyword_rest, parser, visit, arg);
      }
    }

    // block
    {
      if (cast->block == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->block, parser, visit, arg);
      }
    }

    break;
  }
  case PM_PARENTHESES_NODE: {
    pm_parentheses_node_t *cast = (pm_parentheses_node_t *)node;

    // body
    {
      if (cast->body == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->body, parser, visit, arg);
      }
    }
    break;
  }
  case PM_PINNED_EXPRESSION_NODE: {
    pm_pinned_expression_node_t *cast = (pm_pinned_expression_node_t *)node;

    // expression
    { traverse_ast((pm_node_t *)cast->expression, parser, visit, arg); }

    break;
  }
  case PM_PINNED_VARIABLE_NODE: {
    pm_pinned_variable_node_t *cast = (pm_pinned_variable_node_t *)node;

    // variable
    { traverse_ast((pm_node_t *)cast->variable, parser, visit, arg); }

    break;
  }
  case PM_POST_EXECUTION_NODE: {
    pm_post_execution_node_t *cast = (pm_post_execution_node_t *)node;

    // statements
    {
      if (cast->statements == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->statements, parser, visit, arg);
      }
    }

    break;
  }
  case PM_PRE_EXECUTION_NODE: {
    pm_pre_execution_node_t *cast = (pm_pre_execution_node_t *)node;

    // statements
    {
      if (cast->statements == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->statements, parser, visit, arg);
      }
    }

    break;
  }
  case PM_PROGRAM_NODE: {
    pm_program_node_t *cast = (pm_program_node_t *)node;

    // statements
    { traverse_ast((pm_node_t *)cast->statements, parser, visit, arg); }

    break;
  }
  case PM_RANGE_NODE: {
    pm_range_node_t *cast = (pm_range_node_t *)node;

    // left
    {
      if (cast->left == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->left, parser, visit, arg);
      }
    }

    // right
    {
      if (cast->right == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->right, parser, visit, arg);
      }
    }

    break;
  }
  case PM_RATIONAL_NODE: {
    pm_rational_node_t *cast = (pm_rational_node_t *)node;

    // numeric
    { traverse_ast((pm_node_t *)cast->numeric, parser, visit, arg); }

    break;
  }
  case PM_REDO_NODE: {

    break;
  }
  case PM_REGULAR_EXPRESSION_NODE: {
    pm_regular_expression_node_t *cast = (pm_regular_expression_node_t *)node;

    break;
  }
  case PM_REQUIRED_KEYWORD_PARAMETER_NODE: {
    pm_required_keyword_parameter_node_t *cast = (pm_required_keyword_parameter_node_t *)node;

    break;
  }
  case PM_REQUIRED_PARAMETER_NODE: {
    pm_required_parameter_node_t *cast = (pm_required_parameter_node_t *)node;

    break;
  }
  case PM_RESCUE_MODIFIER_NODE: {
    pm_rescue_modifier_node_t *cast = (pm_rescue_modifier_node_t *)node;

    // expression
    { traverse_ast((pm_node_t *)cast->expression, parser, visit, arg); }

    // rescue_expression
    { traverse_ast((pm_node_t *)cast->rescue_expression, parser, visit, arg); }

    break;
  }
  case PM_RESCUE_NODE: {
    pm_rescue_node_t *cast = (pm_rescue_node_t *)node;

    // reference
    {
      if (cast->reference == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->reference, parser, visit, arg);
      }
    }

    // statements
    {
      if (cast->statements == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->statements, parser, visit, arg);
      }
    }

    // consequent
    {
      if (cast->consequent == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->consequent, parser, visit, arg);
      }
    }

    break;
  }
  case PM_REST_PARAMETER_NODE: {
    pm_rest_parameter_node_t *cast = (pm_rest_parameter_node_t *)node;

    break;
  }
  case PM_RETRY_NODE: {

    break;
  }
  case PM_RETURN_NODE: {
    pm_return_node_t *cast = (pm_return_node_t *)node;

    // arguments
    {
      if (cast->arguments == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->arguments, parser, visit, arg);
      }
    }

    break;
  }
  case PM_SELF_NODE: {

    break;
  }
  case PM_SINGLETON_CLASS_NODE: {
    pm_singleton_class_node_t *cast = (pm_singleton_class_node_t *)node;

    // expression
    { traverse_ast((pm_node_t *)cast->expression, parser, visit, arg); }

    // body
    {
      if (cast->body == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->body, parser, visit, arg);
      }
    }

    break;
  }
  case PM_SOURCE_ENCODING_NODE: {

    break;
  }
  case PM_SOURCE_FILE_NODE: {
    pm_source_file_node_t *cast = (pm_source_file_node_t *)node;

    break;
  }
  case PM_SOURCE_LINE_NODE: {

    break;
  }
  case PM_SPLAT_NODE: {
    pm_splat_node_t *cast = (pm_splat_node_t *)node;

    // expression
    {
      if (cast->expression == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->expression, parser, visit, arg);
      }
    }

    break;
  }
  case PM_STATEMENTS_NODE: {
    pm_statements_node_t *cast = (pm_statements_node_t *)node;

    size_t last_index = cast->body.size;
    for (uint32_t index = 0; index < last_index; index++) {
      traverse_ast((pm_node_t *)cast->body.nodes[index], parser, visit, arg);
    }

    break;
  }
  case PM_STRING_NODE: {
    pm_string_node_t *cast = (pm_string_node_t *)node;

    break;
  }
  case PM_SUPER_NODE: {
    pm_super_node_t *cast = (pm_super_node_t *)node;

    // arguments
    {
      if (cast->arguments == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->arguments, parser, visit, arg);
      }
    }

    // block
    {
      if (cast->block == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->block, parser, visit, arg);
      }
    }

    break;
  }
  case PM_SYMBOL_NODE: {
    pm_symbol_node_t *cast = (pm_symbol_node_t *)node;

    break;
  }
  case PM_TRUE_NODE: {

    break;
  }
  case PM_UNDEF_NODE: {
    pm_undef_node_t *cast = (pm_undef_node_t *)node;

    break;
  }
  case PM_UNLESS_NODE: {
    pm_unless_node_t *cast = (pm_unless_node_t *)node;

    // predicate
    { traverse_ast((pm_node_t *)cast->predicate, parser, visit, arg); }

    // statements
    {
      if (cast->statements == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->statements, parser, visit, arg);
      }
    }

    // consequent
    {
      if (cast->consequent == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->consequent, parser, visit, arg);
      }
    }

    break;
  }
  case PM_UNTIL_NODE: {
    pm_until_node_t *cast = (pm_until_node_t *)node;

    // predicate
    { traverse_ast((pm_node_t *)cast->predicate, parser, visit, arg); }

    // statements
    {
      if (cast->statements == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->statements, parser, visit, arg);
      }
    }

    break;
  }
  case PM_WHEN_NODE: {
    pm_when_node_t *cast = (pm_when_node_t *)node;

    // statements
    {
      if (cast->statements == NULL) {
      } else {

        traverse_ast((pm_node_t *)cast->statements, parser, visit, arg);
      }
    }

    break;
  }
  case PM_WHILE_NODE: {
    pm_while_node_t *cast = (pm_while_node_t *)node;

    // predicate
    { traverse_ast((pm_node_t *)cast->predicate, parser, visit, arg); }

    // statements
    {
      if (cast->statements == NULL) {
      } else {
        traverse_ast((pm_node_t *)cast->statements, parser, visit, arg);
      }
    }

    break;
  }
  case PM_X_STRING_NODE: {
    pm_x_string_node_t *cast = (pm_x_string_node_t *)node;

    break;
  }
  case PM_YIELD_NODE: {
    pm_yield_node_t *cast = (pm_yield_node_t *)node;
    // arguments
    {
      if (cast->arguments != NULL) {
        traverse_ast((pm_node_t *)cast->arguments, parser, visit, arg);
      }
    }

    break;
  }
  }
}

void find_node_by_location(pm_node_t *node, pm_parser_t *parser, void *arg) {
  VisitArgs *args = (VisitArgs *)arg;

  if (is_matched_location(node, parser, args->line, args->character)) {
    args->found_node = node;
  }
}

void print_consts(ParsedInfo *parsed_info) {
  if (parsed_info->consts != NULL) {
    for (int i = 0; i < hmlen(parsed_info->consts); i++) {
      printf("Const name: %s\n", parsed_info->consts[i].key);
      for (int j = 0; j < arrlen(parsed_info->consts[i].value->locations); j++) {
        printf("Location: file=%s\n Start line=%zu character=%zu\n End line=%zu character=%zu\n",
               parsed_info->consts[i].value->locations[j].file_path,
               parsed_info->consts[i].value->locations[j].start->line,
               parsed_info->consts[i].value->locations[j].start->character,
               parsed_info->consts[i].value->locations[j].end->line,
               parsed_info->consts[i].value->locations[j].end->character);
      }
      printf("\n");
    }
  }
}
