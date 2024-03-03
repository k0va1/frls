#include "source.h"
#include "stb_ds.h"
#include "utils.h"
#include <stdio.h>

void print_sources(Source **sources) {
  return;
  log_info("Total sources: %d", arrlen(sources));
  log_info("Sources info:");
  Source *source;

  for (long i = 0; i < arrlen(sources); ++i) {
    source = sources[i];
    printf("File path: %s\n", source->file_path);
    printf("Content:\n%s\n", source->content);
    printf("Root ptr: %d\n", source->root);
    printf("Parser ptr:%d\n", source->parser);

    char *opened_status;
    switch (source->open_status) {
    case OPENED:
      opened_status = "OPENED";
      break;
    case CLOSED:
      opened_status = "CLOSED";
      break;
    default:
      opened_status = "UKNOWN";
    }
    printf("Open status: %s\n", opened_status);
    printf("--------------\n");
  }
}
