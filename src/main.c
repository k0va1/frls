#include "parser.h"
#include "prism.h"
#include "source.h"
#include "utils.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

int main() {
  char *src = readall("/Users/k0va1/dev/basecamp_timetrack/lib/basecamp_timetrack/task_summary.rb");

  pm_parser_t parser;
  printf("%s\n", src);
  pm_parser_init(&parser, (const uint8_t *)src, strlen(src), NULL);

  pm_node_t *root = pm_parse(&parser);

  VisitArgs *args = malloc(sizeof(VisitArgs));
  args->found_node = NULL;
  args->line = 28;
  args->character = 22;
  traverse_ast(root, &parser, find_node_by_location, args);

  if (args->found_node != NULL) {
    printf("node %zu\n", PM_NODE_TYPE(args->found_node));
    printf("node %s\n", args->found_node->location.start);
    size_t len = args->found_node->location.end - args->found_node->location.start;
    printf("len %d\n", len);
    pm_constant_id_t c =
        pm_constant_pool_find(&parser.constant_pool, args->found_node->location.start, len);
    printf("const id %zu\n", c);
    pm_constant_t *constant = pm_constant_pool_id_to_constant(&parser.constant_pool, c);
    printf("found node %s\n", strndup((char *)constant->start, constant->length));

    pm_buffer_t buffer = {0};

    pm_prettyprint(&buffer, &parser, root);
    printf("%s\n", buffer.value);
  }

  return 0;
}
