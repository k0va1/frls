#include "config.h"
#include "parser.h"
#include "source.h"

#ifndef SERVER_H_INCLUDED
#define SERVER_H_INCLUDED

typedef enum { UNINITIALIZED, INITIALIZED, SHUTDOWN } SeverStatus;

typedef struct {
  Config *config;
  ParsedInfo *parsed_info;
  Source **sources;
  SeverStatus status;
  int server_socket;
  int client_socket;
  int queue;
  struct sockaddr_in *server_address;
  struct sockaddr_in *client_address;
} Server;

void sync_source(Server *server, char *file_path, char *text);
Server *create_server(Config *config);
void start_server(Server *server);
void destroy_server(Server *server);
void accept_message(Server *server);
#endif
