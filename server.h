#include "config.h"
#include "parser.h"

#ifndef SERVER_H_INCLUDED
#define SERVER_H_INCLUDED

typedef enum { UNINITIALIZED, INITIALIZED, SHUTDOWN } SeverStatus;

typedef struct {
  char *file_path;
  char *content;
} Source;

typedef struct {
  Config *config;
  ParsedInfo *parsed_info;
  Source *sources;
  SeverStatus status;
  int server_socket;
  int client_socket;
  struct sockaddr_in *server_address;
  struct sockaddr_in *client_address;
} Server;

void print_sources(Source *sources);
void sync_source(Server *server, char *file_path, char *text);
Server *create_server(Config *config);
void destroy_server(Server *server);
void accept_message(Server *server, char *buffer);
#endif
