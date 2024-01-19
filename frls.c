#include <fcntl.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "cJSON.h"
#include "commands.h"
#include "config.h"
#include "server.h"
#include "transport.h"
#include "utils.h"

void process_client_message(Server *server, char *client_message) {
  Request *req = create_request(client_message);
  if (req != NULL) {
    char *method = req->method;

    switch (server->status) {
    case INITIALIZED: {
      if (strcmp(method, "initialized") == 0) {
        printf("Method `%s` hasn't implemented yet\n", method);
      } else if (strcmp(method, "shutdown") == 0) {
        shutdown_server(server);
      } else if (strcmp(method, "exit") == 0) {
        exit_server(server);
      } else if (strcmp(method, "textDocument/didOpen") == 0) {
        text_document_did_open(server, req);
      } else if (strcmp(method, "textDocument/didChange") == 0) {
        text_document_did_change(server, req);
      } else if (strcmp(method, "textDocument/didClose") == 0) {
        text_document_did_close(server, req);
      } else {
        fprintf(stderr, "Unsupported method `%s`\n", method);
      }
      break;
    }
    case UNINITIALIZED: {
      if (strcmp(method, "initialize") == 0) {
        initialize(server, req);
      } else {
        uninitialized_error(server);
      }
      break;
    }
    case SHUTDOWN: {
      invalid_request(server);
      break;
    }
    }
    destroy_request(req);
  }
  close(server->client_socket);
}

int main(int argc, char *argv[]) {
  Config *config = create_config(argc, argv);
  Server *server = create_server(config);

  while (true) {
    char client_msg[MESSAGE_BUFFER] = {'\0'};
    accept_message(server, client_msg);
    process_client_message(server, client_msg);
  }

  destroy_server(server);
  destroy_config(config);

  return 0;
}
