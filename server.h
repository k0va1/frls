#include "config.h"
#include "parser.h"

#ifndef SERVER_H_INCLUDED
#define SERVER_H_INCLUDED

typedef enum { UNINITIALIZED, INITIALIZED, SHUTDOWN } SeverStatus;

typedef struct {
  Config *config;
  ParsedInfo *parsed_info;
  SeverStatus status;
  int server_socket;
  int client_socket;
  struct sockaddr_in *server_address;
  struct sockaddr_in *client_address;
} Server;

Server *create_server(Config *config);
void destroy_server(Server *server);
void accept_message(Server *server, char *buffer);
#endif
