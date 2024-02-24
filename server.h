#include "config.h"
#include "parser.h"
#include "source.h"
#include "transport.h"

#ifndef SERVER_H_INCLUDED
#define SERVER_H_INCLUDED

#define MAX_CONNECTIONS 8

typedef enum { UNINITIALIZED, INITIALIZED, SHUTDOWN } SeverStatus;

typedef struct {
  Config *config;
  ParsedInfo *parsed_info;
  Source **sources;
  SeverStatus status;
  SOCKET server_socket;
  Client *clients;
  fd_set *master_set;
  fd_set *working_set;
} Server;

void sync_source(Server *server, char *file_path, char *text);
Server *create_server(Config *config);
void uninitialized_error(Client *client);
void invalid_request(Client *client);
void send_error(Client *client, char *error_msg, int err_code);
void start_server(Server *server);
void destroy_server(Server *server);
void accept_message(Server *server);
#endif
