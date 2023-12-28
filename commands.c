#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "commands.h"
#include "parser.h"
#include "transport.h"
#include "utils.h"

#define SUPPORTED_FILE_EXTENSIONS ".rb"

void initialize(Server *server, Request *request) {
  log_info("Initializing...");

  Config *config = server->config;

  const cJSON *process_id =
      cJSON_GetObjectItemCaseSensitive(request->params, "processId");
  if (cJSON_IsNumber(process_id)) {
    config->parent_process_id = process_id->valueint;
  }
  const cJSON *client_info =
      cJSON_GetObjectItemCaseSensitive(request->params, "clientInfo");
  if (client_info != NULL) {
    const cJSON *client_name =
        cJSON_GetObjectItemCaseSensitive(client_info, "name");
    if (cJSON_IsString(client_name) && (client_name->valuestring != NULL)) {
      config->client_name = strdup(client_name->valuestring);
    }
    const cJSON *client_version =
        cJSON_GetObjectItemCaseSensitive(client_info, "version");
    if (cJSON_IsString(client_version) &&
        (client_version->valuestring != NULL)) {
      config->client_version = strdup(client_version->valuestring);
    }
    const cJSON *root_uri =
        cJSON_GetObjectItemCaseSensitive(request->params, "rootUri");
    if (cJSON_IsString(root_uri) && (root_uri->valuestring != NULL)) {
      config->project_root = strdup(root_uri->valuestring);
    }

    const cJSON *capabilities =
        cJSON_GetObjectItemCaseSensitive(request->params, "capabilities");
    if (cJSON_IsObject(capabilities)) {
      config->client_capabilities = cJSON_Duplicate(capabilities, true);
    }

    if (config->project_root) {
      char *file_path = get_file_path(config->project_root);
      log_info("File path: %s", file_path);
      if (is_dir(file_path) == 1) {
        log_info("To be implemented");
      } else if (is_file(file_path) == 1) {
        if (is_ends_with(file_path, SUPPORTED_FILE_EXTENSIONS)) {
          parse(file_path, server->parsed_info);
        } else {
          log_error("Unsupported file type: %s. Server supports only files "
                    "with extensions: %s",
                    file_path, SUPPORTED_FILE_EXTENSIONS);
        }
      }
    }
  }

  server->status = INITIALIZED;
}

void shutdown_server(Server *server, Request *request) {
  log_info("Shutting down");
  server->status = SHUTDOWN;
  send_response(server->client_socket, 200, "");
}

void exit_server(Server *server, Request *request) {
  log_info("Exiting");
  if (server->status == SHUTDOWN) {
    exit(0);
  } else {
    exit(1);
  }
}

void text_document_did_open(Config *config, Request *request) {
  log_info("Document was opened");
}

void text_document_did_change(Config *config, Request *request) {
  log_info("Document was changed");
}

void text_document_did_close(Config *config, Request *request) {
  log_info("Document was closed");
}
