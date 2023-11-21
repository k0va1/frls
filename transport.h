#include "stdlib.h"
#include "./cJSON.h"

#ifndef TRANSPORT_H_INCLUDED
#define TRANSPORT_H_INCLUDED

#define DEFAULT_CHARSET "utf-8"

typedef struct {
  char *content_type;
  char *charset;
  size_t *content_length;
} Headers;

typedef struct {
  Headers *headers;
  char *method;
  cJSON *params;
} Request;

Headers *create_headers(char *headers_str);
Request *create_request(char *headers_str);

void destroy_headers(Headers *headers);
void destroy_request(Request *Request);

#endif
