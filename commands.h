#include "config.h"
#include "server.h"
#include "stdlib.h"
#include "transport.h"

#ifndef COMMANDS_H_INCLUDED
#define COMMANDS_H_INCLUDED

void initialize(Server *server, Client *client, Request *request);
void initialized(Server *server, Client *client);
void shutdown_server(Server *server, Client *client);
void exit_server(Server *server);

void process_file(Server *server, char *file_path);
void process_file_tree(Server *server, char *root_path);
void text_document_did_open(Server *server, Client *client, Request *request);
void text_document_did_change(Server *server, Client *client, Request *request);
void text_document_did_close(Server *server, Request *request);
void go_to_definition(Server *server, Client *client, Request *request);
Location *get_locations_by_position(Server *server, Source *source, size_t line, size_t character);
pm_node_t *get_node_by_position(Source *source, size_t line, size_t character);

#endif
