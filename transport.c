#include "transport.h"
#include "utils.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

Headers *create_headers(char *headers_str) {
  Headers *headers = malloc(sizeof(Headers));
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
  if(headers->charset == NULL) {
    headers->charset = strdup(DEFAULT_CHARSET);
  }

  return headers;
}

Request *create_request(char *request) {
  char *content_separator = "\r\n\r\n";
  char *tail;

  tail = strstr(request, content_separator);
  if (tail == NULL) {
    log_error("Invalid message");
    return NULL;
  }
  size_t headers_len = tail - request + 2;
  size_t body_len = strlen(request) - headers_len;

  char headers_str[headers_len];
  char body_str[body_len];

  strncpy(headers_str, request, headers_len);
  strncpy(body_str, request + headers_len, body_len);

  Headers *headers = create_headers(headers_str);
  log_info("Content type: %s", headers->content_type);
  log_info("Charset: %s", headers->charset);
  log_info("Content length: %zu", *headers->content_length);

  if (strcasecmp(headers->charset, DEFAULT_CHARSET) != 0) {
    log_error("Unsupported charset: %s. Tool supports utf-8 only\n",
              headers->charset);
    destroy_headers(headers);
    return NULL;
  }
  cJSON *body = cJSON_Parse(body_str);
  if (body == NULL) {
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr != NULL) {
      log_error("Invalid JSON at: %s", error_ptr);
      destroy_headers(headers);
      cJSON_Delete(body);
      return NULL;
    }
  }

  Request *req = malloc(sizeof(Request));
  req->headers = headers;

  cJSON *method = cJSON_GetObjectItem(body, "method");
  if (cJSON_IsString(method) && (method->valuestring != NULL)) {
    req->method = strdup(method->valuestring);
  }

  cJSON *params = cJSON_GetObjectItem(body, "params");
  if (cJSON_IsObject(params)) {
    req->params = cJSON_Duplicate(params, true);
  }

  return req;
}

void send_response(int socket, int status, char *body) {
  size_t content_length = strlen(body);
  char *template = "HTTP/1.1 %d %s\r\nContent-Length: %d\r\nContent-Type: "
                   "application/vscode-jsonrpc; charset=utf-8\r\n\r\n\%s\n";
  char *status_msg;
  switch (status) {
  case 200:
    status_msg = "OK";
    break;
  case 204:
    status_msg = "No Content";
    break;
  default:
    log_error("Unsupported status %d", status);
    return;
  }

  char response_message[MESSAGE_BUFFER_SIZE] = {'\0'};
  int msg_len = snprintf(response_message, MESSAGE_BUFFER_SIZE, template, status,
                         status_msg, content_length, body);
  if (send(socket, response_message, msg_len, 0) < 0) {
    log_error("Couldn't send response because of %s", strerror(errno));
  }
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
