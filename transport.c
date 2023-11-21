#include "transport.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Headers *create_headers(char *headers_str) {
  Headers *headers = malloc(sizeof(*headers));
  char *separator = "\r\n";

  char *tail = strstr(headers_str, separator);
  char buff[strlen(headers_str)];

  while (tail != NULL) {
    strncpy(buff, headers_str, tail - headers_str);
    buff[tail - headers_str] = '\0';
    char *header_name = strtok(buff, ":");

    if (strcasecmp(header_name, "Content-Length") == 0) {
      size_t content_length = atoi(trim(strtok(NULL, ":")));
      headers->content_length = malloc(sizeof(size_t));
      *headers->content_length = content_length;
    } else if (strcasecmp(header_name, "Content-Type") == 0) {
      char *header_value = strtok(NULL, ":");

      char *content_type = trim(strtok(header_value, ";"));
      headers->content_type = strdup(content_type);

      char *charset_kv = strtok(NULL, ";");
      char *charset;
      if (charset_kv == NULL) {
        charset = DEFAULT_CHARSET;
      } else {
        strtok(charset_kv, "=");
        charset = strtok(NULL, "=");
        if (charset == NULL) {
          charset = DEFAULT_CHARSET;
        }
      }
      headers->charset = strdup(charset);
    }
    headers_str = tail + strlen(separator);
    tail = strstr(headers_str, "\r\n");
  }
  return headers;
}

Request *create_request(char *request) {
  char *content_separator = "\r\n\r\n";
  char *tail;

  tail = strstr(request, content_separator);
  if (tail == NULL) {
    fprintf(stderr, "Invalid message");
    return NULL;
  }
  size_t headers_len = tail - request + 2;
  size_t body_len = strlen(request) - headers_len;

  char headers_str[headers_len];
  char body_str[body_len];

  strncpy(headers_str, request, headers_len);
  strncpy(body_str, request + headers_len, body_len);

  Headers *headers = create_headers(headers_str);
  printf("Content type: %s\n", headers->content_type);
  printf("Charset: %s\n", headers->charset);
  printf("Content length: %zu\n", *headers->content_length);

  if (strcasecmp(headers->charset, DEFAULT_CHARSET) != 0) {
    fprintf(stderr, "Unsupported charset: %s. Tool supports utf-8 only\n",
            headers->charset);
    return NULL;
  }
  cJSON *params = cJSON_Parse(body_str);
  char *method = cJSON_GetObjectItem(params, "method")->valuestring;

  Request *req = malloc(sizeof(Request));
  req->headers = headers;
  req->method = strdup(method);
  req->params = params;
  return req;
}

void destroy_headers(Headers *headers) {
  free(headers->content_type);
  free(headers->charset);
  free(headers->content_length);
  free(headers);
}

void destroy_request(Request *req) {
  destroy_headers(req->headers);
  free(req->method);
  cJSON_free(req->params);
  free(req);
}
