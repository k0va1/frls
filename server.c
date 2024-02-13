#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "cJSON.h"
#include "commands.h"
#include "config.h"
#include "server.h"
#include "transport.h"
#include "utils.h"

Server *create_server(Config *config) {
  Server *server = malloc(sizeof(Server));
  server->config = config;
  server->status = UNINITIALIZED;
  server->parsed_info = malloc(sizeof(server->parsed_info));
  server->sources = NULL;

  if ((server->server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    fail("Couldn't initialize socket");
  }

  log_info("Socket initialized");

  int option_value = 1;
  if (setsockopt(server->server_socket, SOL_SOCKET, SO_REUSEADDR, &option_value,
                 sizeof(option_value)) < 0) {
    fail("Couldn't set socket options");
  }

  struct sockaddr_in server_address = {.sin_family = AF_INET,
                                       .sin_port = htons(config->port),
                                       .sin_addr.s_addr =
                                           inet_addr(config->host),
                                       .sin_zero[7] = '\0'};

  server->server_address = &server_address;

  if (bind(server->server_socket, (struct sockaddr *)server->server_address,
           sizeof(struct sockaddr)) < 0) {
    fail("Couldn't bind the socket");
  }

  if (listen(server->server_socket, 4) < 0) {
    fail("Couldn't listen to connections");
  }

  log_info("Listening on %s:%d", config->host, config->port);

  return server;
}

void accept_message(Server *server) {
  uint length_of_address = sizeof(struct sockaddr);
  server->client_socket =
      accept(server->server_socket, (struct sockaddr *)server->client_address,
             &length_of_address);

  if (server->client_socket < 0) {
    fail("Couldn't establish connection with client");
  }
}

void read_message(Server *server, char *buffer) {
  int bytes_read = read(server->client_socket, buffer, MESSAGE_BUFFER_SIZE);

  if (bytes_read < 0) {
    fail("Couldn't read client message");
  } else if (bytes_read == 0) {
    // exit on empty message(Ctrl-C in telnet)
    exit(1);
  } else {
    buffer[bytes_read] = '\0';
    log_info("Message from client:\n%s", buffer);
  }
}

void process_client_message(Server *server, char *client_message) {
  Request *req = create_request(client_message);
  if (req != NULL) {
    char *method = req->method;

    switch (server->status) {
    case INITIALIZED: {
      if (strcmp(method, "initialized") == 0) {
        initialized(server);
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

void start_server(Server *server) {
  while(true) {

  }
}

void destroy_server(Server *server) { free(server); }
