#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>

#include "cJSON.h"
#include "commands.h"
#include "config.h"
#include "server.h"
#include "transport.h"
#include "utils.h"

void drop_client(Server *server, Client *client) {
  close(client->socket);

  Client **p = &server->clients;
  while (*p) {
    if (*p == client) {
      *p = client->next;
      free(client);
      log_info("Client dropped");
      return;
    }
    p = &(*p)->next;
  }

  fail("drop_client not found");
}

Client *create_client() {
  Client *client = (Client *)calloc(1, sizeof(Client));
  if (!client) {
    fail("Out of memory");
  }
  client->address_length = sizeof(client->address);
  return client;
}

Client *add_client(Server *server) {
  Client *client = create_client();
  client->next = server->clients;
  client->socket =
      accept(server->server_socket, (struct sockaddr *)&(client->address),
             &(client->address_length));

  if (client->socket < 0) {
    fail("accept() failed");
  }

  server->clients = client;
  return client;
}

void wait_on_clients(Server *server) {
  memcpy(server->working_set, server->master_set, sizeof(fd_set));
  SOCKET max_socket = server->server_socket;

  struct Client *client = server->clients;
  while (client) {
    FD_SET(client->socket, server->working_set);
    if (client->socket > max_socket)
      max_socket = client->socket;
    client = client->next;
  }
  if (select(max_socket + 1, server->working_set, NULL, NULL, NULL) < 0) {
    fail("select() failed");
  }
}

void send_error(Client *client, char *error_msg, int err_code) {
  cJSON *jsonrpc = NULL;
  cJSON *req_id = NULL;
  cJSON *code = NULL;
  cJSON *message = NULL;

  cJSON *body = cJSON_CreateObject();
  cJSON *error = cJSON_CreateObject();

  code = cJSON_CreateNumber(err_code);
  log_error(error_msg);
  message = cJSON_CreateString(error_msg);
  cJSON_AddItemToObject(error, "code", code);
  cJSON_AddItemToObject(error, "message", message);

  jsonrpc = cJSON_CreateString("2.0");
  req_id = cJSON_CreateNumber(1);

  cJSON_AddItemToObject(body, "jsonrpc", jsonrpc);
  cJSON_AddItemToObject(body, "id", req_id);
  cJSON_AddItemToObject(body, "error", error);

  char *json_str = cJSON_PrintUnformatted(body);
  send_response(client->socket, 200, json_str);
  cJSON_Delete(body);
}

void uninitialized_error(Client *client) {
  send_error(
      client,
      "Server hasn't been initialized yet. Send `initialize` request first: "
      "https://microsoft.github.io/language-server-protocol/specifications/lsp/"
      "3.17/specification/#initialize",
      SERVER_NOT_INITIALIZED);
}

void invalid_request(Client *client) {
  send_error(client,
             "Server has been shutted down: "
             "https://microsoft.github.io/language-server-protocol/"
             "specifications/lsp/3.17/specification/#shutdown",
             INVALID_REQUEST);
}

void process_client_message(Server *server, Client *client) {
  Request *req = create_request(client->request);
  if (req != NULL) {
    char *method = req->method;

    log_info("method: %s", method);
    switch (server->status) {
    case INITIALIZED: {
      if (strcmp(method, "initialized") == 0) {
        initialized(server, client);
      } else if (strcmp(method, "shutdown") == 0) {
        shutdown_server(server, client);
      } else if (strcmp(method, "exit") == 0) {
        exit_server(server);
      } else if (strcmp(method, "textDocument/didOpen") == 0) {
        text_document_did_open(server, client, req);
      } else if (strcmp(method, "textDocument/didChange") == 0) {
        text_document_did_change(server, client, req);
      } else if (strcmp(method, "textDocument/didClose") == 0) {
        text_document_did_close(server, req);
      } else {
        fprintf(stderr, "Unsupported method `%s`\n", method);
      }
      break;
    }
    case UNINITIALIZED: {
      if (strcmp(method, "initialize") == 0) {
        initialize(server, client, req);
      } else {
        uninitialized_error(client);
      }
      break;
    }
    case SHUTDOWN: {
      invalid_request(client);
      break;
    }
    }
    destroy_request(req);
  }
}

void create_bind_address(struct addrinfo **bind_address, Config *config) {
  log_info("Creating bind address...");
  struct addrinfo *hints = calloc(1, sizeof(struct addrinfo));
  hints->ai_family = AF_INET;
  hints->ai_socktype = SOCK_STREAM;
  hints->ai_flags = AI_PASSIVE;
  hints->ai_canonname = config->host;

  char port[10];
  sprintf(port, "%u", config->port);
  getaddrinfo(NULL, port, hints, bind_address);
}

void create_socket(struct addrinfo *bind_address, Server *server) {
  log_info("Creating socket...");
  server->server_socket =
      socket(bind_address->ai_family, bind_address->ai_socktype,
             bind_address->ai_protocol);
  if (server->server_socket < 0) {
    fail("Couldn't initialize socket");
  }

  // Allow socket descriptor to be reuseable
  int option = 1;
  log_info("Setting socket options...");
  if (setsockopt(server->server_socket, SOL_SOCKET, SO_REUSEADDR,
                 (char *)&option, sizeof(option)) < 0) {
    close(server->server_socket);
    fail("Couldn't set socket options");
  }

  // Set socket to be nonblocking
  if (ioctl(server->server_socket, FIONBIO, (char *)&option) < 0) {
    close(server->server_socket);
    fail("Couldn't set socket to be nonblocking");
  }
}

void bind_and_listen(struct addrinfo *bind_address, Server *server) {
  log_info("Binding socket...");
  if (bind(server->server_socket, bind_address->ai_addr,
           bind_address->ai_addrlen) < 0) {
    close(server->server_socket);
    fail("Couldn't bind the socket");
  }
  freeaddrinfo(bind_address);

  if (listen(server->server_socket, MAX_CONNECTIONS) < 0) {
    close(server->server_socket);
    fail("Couldn't listen to connections");
  }

  // add socket to set
  FD_ZERO(server->master_set);
  FD_SET(server->server_socket, server->master_set);

  log_info("Listening on %s:%d", server->config->host, server->config->port);
}

void cleanup_sockets(Server *server) {
  for (int i = 0; i <= server->server_socket; ++i) {
    if (FD_ISSET(i, server->master_set))
      close(i);
  }
}

Server *create_server(Config *config) {
  Server *server = malloc(sizeof(Server));
  server->config = config;
  server->status = UNINITIALIZED;
  server->parsed_info = malloc(sizeof(server->parsed_info));
  server->sources = NULL;
  server->clients = NULL;
  server->master_set = malloc(sizeof(struct fd_set));
  server->working_set = malloc(sizeof(struct fd_set));

  struct addrinfo *bind_address;
  create_bind_address(&bind_address, config);
  create_socket(bind_address, server);
  bind_and_listen(bind_address, server);

  return server;
}

void start_server(Server *server) {
  while (true) {
    wait_on_clients(server);

    if (FD_ISSET(server->server_socket, server->working_set)) {
      add_client(server);
    } else {
      Client *client = server->clients;
      while (client) {
        if (FD_ISSET(client->socket, server->working_set)) {
          if (client->received >= MESSAGE_BUFFER_SIZE) {
            log_error("message to big, rejected");
            client = client->next;
            continue;
          }
          memset(client->request, '\0', MESSAGE_BUFFER_SIZE);
          int bytes_received =
              recv(client->socket, client->request, MESSAGE_BUFFER_SIZE, 0);

          if (bytes_received == 0) {
            log_error("Unexpected disconnect");
            drop_client(server, client);
          } else if (bytes_received == -1) {
            log_error("Unexpected error");
            drop_client(server, client);
          } else {
            process_client_message(server, client);
          }
        }
        client = client->next;
      }
    }
  }
}

void destroy_server(Server *server) { free(server); }
