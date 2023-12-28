#include "stdlib.h"
#include "transport.h"
#include "config.h"
#include "server.h"

#ifndef COMMANDS_H_INCLUDED
#define COMMANDS_H_INCLUDED

void initialize(Server *server, Request *request);
void shutdown_server(Server *server, Request *request);
void exit_server(Server *server, Request *request);

void text_document_did_open(Config *config, Request *request);
void text_document_did_change(Config *config, Request *request);
void text_document_did_close(Config *config, Request *request);

#endif
