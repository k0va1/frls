#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <regex.h>

#define PORT 1488

void fail(char *msg) {
  fprintf(stderr, "[ERROR] %s: %s", msg, strerror(errno));
  exit(-1);
}

void info(char *msg) {
  printf("[INFO] %s\n", msg);
}

void process_client_message(char *client_message) {
  char *headers;
  char *body;
  char *content_length_header;

  char *token;
  int content_length;

  char *content_type_header;
  char *content_type;

  char *content_separator = "\r\n";
  token = strtok(client_message, content_separator);

  while(token != NULL) {
    printf("HEADER: %s\n", token);
    token = strtok(NULL, content_separator);
  }
/*  body = client_message;*/

  /*content_length_header = strtok(headers, "\n");*/
  /*content_type_header = headers;*/
}

int main(int argc, char *argv[]) {
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
  server_address.sin_port = htons(PORT);
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_zero[7] = '\0';

  if (bind(socket_fd, (struct sockaddr *)&server_address, sizeof(struct sockaddr)) <
      0) {
    fail("Couldn't bind the socket");
  }

  if (listen(socket_fd, 4) < 0) {
    fail("Couldn't listen connections");
  }
  printf("[INFO] Listening on port: %d\n", PORT);

  uint length_of_address = sizeof(client_address);
  int client_socket = accept(socket_fd, (struct sockaddr*)&client_address, &length_of_address);

  if (client_socket < 0) {
    fail("Couldn't establish connection with client");
  }

  while(true) {
    char client_msg[1024];
    int bytes_read = read(client_socket, client_msg, sizeof(client_msg));

    if(bytes_read < 0) {
      printf("[ERROR] Couldn't read client message\n");
      break;
    } else {
      client_msg[bytes_read] = '\0';
      printf("[INFO] Message from client: %s\n", client_msg);
      process_client_message(client_msg);
    }
  }

  close(socket_fd);
  close(client_socket);

  return 0;
}
