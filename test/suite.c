#include "commands.h"
#include "prism.h"
#include <assert.h>

char *get_node_name(Source *source, pm_node_t *node) {
  pm_constant_read_node_t *cast = (pm_constant_read_node_t *)node;
  pm_line_column_t start = pm_newline_list_line_column(
      &source->parser->newline_list, node->location.start, source->parser->start_line);
  pm_constant_t *constant =
      pm_constant_pool_id_to_constant(&source->parser->constant_pool, cast->name);
  return strndup((char *)constant->start, constant->length);
}

void get_node_by_position_test() {
  char *src = "module Project\n"
              "  class Error < StandardError\n"
              "    [12]\n"
              "\n"
              "  end\n"
              "end";
  Source source = {
      .content = src,
      .file_path = "hello.rb",
  };
  pm_parser_t *parser = malloc(sizeof(pm_parser_t));
  pm_parser_init(parser, (const uint8_t *)source.content, strlen(source.content), NULL);

  pm_node_t *root = pm_parse(parser);
  source.root = root;
  source.parser = parser;

  pm_node_t *node = get_node_by_position(&source, 1, 8);
  assert(node != NULL);
  char *node_name = get_node_name(&source, node);
  assert(strcmp(node_name, "Error") == 0);

  node = get_node_by_position(&source, 0, 9);
  assert(node_name != NULL);
  node_name = get_node_name(&source, node);
  assert(strcmp(node_name, "Project") == 0);

  node = get_node_by_position(&source, 1, 17);
  assert(node_name != NULL);
  node_name = get_node_name(&source, node);
  assert(strcmp(node_name, "StandardError") == 0);
}

int main(int argc, char **argv) {
  printf("Tests started...\n");

  get_node_by_position_test();

  printf("Tests finished...\n");
}
