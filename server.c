#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "cJSON.h"
#include "commands.h"
#include "config.h"
#include "server.h"
#include "transport.h"
#include "utils.h"

const int kReadEvent = 1;
const int kWriteEvent = 2;

void set_non_block(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0)
    fail("Could not read server socket flags");
  int r = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  if (r < 0)
    fail("Could set server socket to be non blocking");
}

void update_events(int efd, int fd, int events, bool modify) {
  log_info("updating events");

  struct kevent ev[2];
  int n = 0;
  if (events & kReadEvent) {
    EV_SET(&ev[n++], fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0,
           (void *)(intptr_t)fd);
  } else if (modify) {
    EV_SET(&ev[n++], fd, EVFILT_READ, EV_DELETE, 0, 0, (void *)(intptr_t)fd);
  }
  if (events & kWriteEvent) {
    EV_SET(&ev[n++], fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0,
           (void *)(intptr_t)fd);
  } else if (modify) {
    EV_SET(&ev[n++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, (void *)(intptr_t)fd);
  }
  printf("%s fd %d events read %d write %d\n", modify ? "mod" : "add", fd,
         events & kReadEvent, events & kWriteEvent);
  int r = kevent(efd, ev, n, NULL, 0, NULL);
  if (r < 0)
    fail("kevent failed");
}

Server *create_server(Config *config) {
  Server *server = malloc(sizeof(Server));
  server->config = config;
  server->status = UNINITIALIZED;
  server->parsed_info = malloc(sizeof(server->parsed_info));
  server->sources = NULL;

  if ((server->queue = kqueue()) < 0) {
    fail("Couldn't create queue");
  }

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

  set_non_block(server->server_socket);
  update_events(server->queue, server->server_socket, kReadEvent, false);

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

  set_non_block(server->client_socket);
  update_events(server->queue, server->client_socket, kReadEvent | kWriteEvent,
                false);
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
  const int kMaxEvents = 20;
  struct kevent activeEvs[kMaxEvents];
  const int waitms = 10000; // 10s
  struct timespec timeout;
  timeout.tv_sec = waitms / 1000;
  timeout.tv_nsec = (waitms % 1000) * 1000 * 1000;

  while (true) {
    char client_msg[MESSAGE_BUFFER_SIZE] = {'\0'};

    int n = kevent(server->queue, NULL, 0, activeEvs, kMaxEvents, &timeout);

    for (int i = 0; i < n; i++) {
      int fd = (int)(intptr_t)activeEvs[i].udata;
      int events = activeEvs[i].filter;
      if (events == EVFILT_READ) {
        if (fd == server->server_socket) {
          accept_message(server);
        } else {
          read_message(server, client_msg);
          process_client_message(server, client_msg);
        }
      } else if (events == EVFILT_WRITE) {
        //       handleWrite(efd, fd);
      } else {
        fail("Unknown event");
      }
    }

    //    process_client_message(server, client_msg);
  }
}

void destroy_server(Server *server) { free(server); }
