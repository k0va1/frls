#include "stdlib.h"
#include "transport.h"
#include "config.h"
#include "server.h"

#ifndef COMMANDS_H_INCLUDED
#define COMMANDS_H_INCLUDED

void initialize(Server *server, Request *request);
void shutdown_server(Server *server);
void exit_server(Server *server);

void process_file(Server *server, char *file_path);
void process_file_tree(Server *server, char *root_path);
void text_document_did_open(Server *server, Request *request);
void text_document_did_change(Server *server, Request *request);
void text_document_did_close(Server *server, Request *request);

#endif
