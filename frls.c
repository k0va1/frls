#include <fcntl.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "cJSON.h"
#include "config.h"
#include "transport.h"
#include "commands.h"
#include "utils.h"

#define MESSAGE_BUFFER 64000

void process_client_message(Config *config, char *client_message) {
  Request *req = create_request(client_message);
  if (req != NULL) {
    char *method = req->method;

    if (strcmp(method, "initialize") == 0) {
      initialize(config, req);
    } else if (strcmp(method, "initialized") == 0) {
      printf("Method `%s` hasn't implemented yet\n", method);
    } else if (strcmp(method, "shutdown") == 0) {
      printf("Method `%s` hasn't implemented yet\n", method);
    } else if (strcmp(method, "exit") == 0) {
      printf("Method `%s` hasn't implemented yet\n", method);
    } else {
      fprintf(stderr, "Unsupported method `%s`\n", method);
    }

    destroy_request(req);
  }
}

int main(int argc, char *argv[]) {
  Config *config = create_config(argc, argv);

  int socket_fd;
  struct sockaddr_in server_address;
  struct sockaddr_in client_address;

  if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    fail("Couldn't initialize socket");
  }

  info("Socket initialized");

  int option_value = 1;
  if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &option_value,
                 sizeof(option_value)) < 0) {
    fail("Couldn't set socket options");
  }

  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(config->port);
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_zero[7] = '\0';

  if (bind(socket_fd, (struct sockaddr *)&server_address,
           sizeof(struct sockaddr)) < 0) {
    fail("Couldn't bind the socket");
  }

  if (listen(socket_fd, 4) < 0) {
    fail("Couldn't listen to connections");
  }
  info("Listening on %s:%d", config->host, config->port);

  uint length_of_address = sizeof(client_address);

  while (true) {
    int client_socket = accept(socket_fd, (struct sockaddr *)&client_address,
                               &length_of_address);

    if (client_socket < 0) {
      fail("Couldn't establish connection with client");
    }

    char client_msg[MESSAGE_BUFFER];
    int bytes_read = read(client_socket, client_msg, sizeof(client_msg));

    if (bytes_read < 0) {
      error("Couldn't read client message");
      break;
    } else if (bytes_read == 0) {
      // exit on empty message(Ctrl-C in telnet)
      break;
    } else {
      client_msg[bytes_read] = '\0';
      info("Message from client:\n%s", client_msg);
      process_client_message(config, client_msg);
    }
    close(client_socket);
  }
  close(socket_fd);

  return 0;
}
