#ifndef SOURCE_H_INCLUDED
#define SOURCE_H_INCLUDED

typedef enum { OPENED, CLOSED } OpenStatus;

typedef struct {
  char *file_path;
  char *content;
  OpenStatus open_status;
} Source;

void print_sources(Source **sources);

#endif
