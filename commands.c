#include "commands.h"
#include "parser.h"
#include "source.h"
#include "stb_ds.h"
#include "transport.h"
#include "utils.h"
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *SUPPORTED_FILE_EXTENSIONS[] = {".rb"};
static const char *SUPPORTED_LANGUAGE_IDS[] = {"ruby"};

Source *add_source(Server *server, char *file_path, char *content) {
  Source *source = malloc(sizeof(Source));
  source->file_path = strdup(file_path);
  source->content = strdup(content);
  arrput(server->sources, source);
  return source;
}

Source *get_source(Server *server, char *file_path) {
  Source *source;

  for (long i = 0; i < arrlen(server->sources); ++i) {
    source = server->sources[i];
    if (strcmp(file_path, source->file_path)) {
      return source;
    }
  }
  return NULL;
}

void sync_source(Server *server, char *file_path, char *content) {
  log_info("Syncing file %s...", file_path);

  Source *source = get_source(server, file_path);
  if (source) {
    source->content = content;
    log_info("Source updated");
  } else {
    source = add_source(server, file_path, content);
    source->open_status = OPENED;
    log_info("New source added");
  }
}

void process_file(Server *server, char *file_path) {
  log_info("Processing file `%s`", file_path);
  if (is_includes(SUPPORTED_FILE_EXTENSIONS, file_ext(file_path))) {
    char *content = readall(file_path);
    Source *source = add_source(server, file_path, content);
    source->open_status = CLOSED;
    parse(source, server->parsed_info);
  } else {
    log_error("Unsupported file type: %s. Server supports only files "
              "with extensions: `%s`",
              file_path, SUPPORTED_FILE_EXTENSIONS[0]);
  }
}

void process_content(Server *server, char *file_path, char *content) {
  log_info("Processing file `%s`", file_path);
  Source *source = get_source(server, file_path);
  sync_source(server, file_path, content);
  parse(source, server->parsed_info);
}

void process_file_tree(Server *server, char *root_path) {
  DIR *opened_dir;
  struct dirent *dir;
  opened_dir = opendir(root_path);

  if (opened_dir) {
    while ((dir = readdir(opened_dir)) != NULL) {
      char file_path[PATH_MAX + 1];
      sprintf(file_path, "%s/%s", root_path, dir->d_name);

      if (dir->d_type == DT_DIR) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
          continue;
        }
        process_file_tree(server, file_path);
      }

      if (dir->d_type == DT_REG) {
        process_file(server, file_path);
      }
    }
    closedir(opened_dir);
  } else {
    log_error("Error while reading `%s` because of error: '%s'", root_path,
              strerror(errno));
  }
}

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
      if (is_dir(file_path)) {
        process_file_tree(server, file_path);
      } else if (is_file(file_path)) {
        process_file(server, file_path);
      }
    }
  }

  server->status = INITIALIZED;
  log_info("Server initialized");
  print_config(server->config);
  print_sources(server->sources);
}

void shutdown_server(Server *server) {
  log_info("Shutting down");
  server->status = SHUTDOWN;
  send_response(server->client_socket, 200, "");
}

void exit_server(Server *server) {
  log_info("Exiting");
  if (server->status == SHUTDOWN) {
    exit(0);
  } else {
    exit(1);
  }
}

void text_document_did_open(Server *server, Request *request) {
  log_info("Opening document...");
  const cJSON *text_document =
      cJSON_GetObjectItemCaseSensitive(request->params, "textDocument");

  if (cJSON_IsObject(text_document)) {
    char *uri;
    char *language_id;
    char *text;

    const cJSON *json_uri =
        cJSON_GetObjectItemCaseSensitive(text_document, "uri");
    if (cJSON_IsString(json_uri) && (json_uri->valuestring != NULL)) {
      uri = strdup(json_uri->valuestring);
    } else {
      log_error("Couldn't parse URI");
      return;
    }

    const cJSON *json_language_id =
        cJSON_GetObjectItemCaseSensitive(text_document, "languageId");
    if (cJSON_IsString(json_language_id) &&
        (json_language_id->valuestring != NULL)) {
      language_id = strdup(json_language_id->valuestring);
    } else {
      log_error("Couldn't parse language id");
      return;
    }

    const cJSON *json_text =
        cJSON_GetObjectItemCaseSensitive(text_document, "text");
    if (cJSON_IsString(json_text) && (json_text->valuestring != NULL)) {
      text = strdup(json_text->valuestring);
    } else {
      log_error("Couldn't parse text");
      return;
    }

    if (is_includes(SUPPORTED_LANGUAGE_IDS, language_id) && uri != NULL) {
      char *file_path = get_file_path(uri);
      Source *source = get_source(server, file_path);
      if (source) {
        if (source->open_status != OPENED) {
          process_content(server, file_path, text);
          source->open_status = OPENED;
        } else {
          log_error("Source has already been opened");
        }
      } else {
        source = add_source(server, file_path, text);
        source->open_status = OPENED;
      }
      print_sources(server->sources);
    }
  } else {
    log_error("Couldn't parse text document");
    return;
  }
}

void text_document_did_change(Server *server, Request *request) {
  log_info("Changing document...");
  const cJSON *text_document =
      cJSON_GetObjectItemCaseSensitive(request->params, "textDocument");
  const cJSON *content_changes =
      cJSON_GetObjectItemCaseSensitive(request->params, "contentChanges");

  char *uri;
  if (cJSON_IsObject(text_document)) {
    const cJSON *json_uri =
        cJSON_GetObjectItemCaseSensitive(text_document, "uri");
    if (cJSON_IsString(json_uri) && (json_uri->valuestring != NULL)) {
      uri = strdup(json_uri->valuestring);
    } else {
      log_error("Couldn't parse URI");
      return;
    }
  } else {
    log_error("Couldn't parse text document");
    return;
  }

  if (cJSON_IsArray(content_changes)) {
    cJSON *element = NULL;
    // it's supported full document update only atm, so array should contain
    // only one element with `text` key
    cJSON_ArrayForEach(element, content_changes) {
      cJSON *json_text = cJSON_GetObjectItemCaseSensitive(element, "text");

      if (cJSON_IsString(json_text) && (json_text->valuestring != NULL)) {
        char *text = json_text->valuestring;
        char *file_path = get_file_path(uri);
        Source *source = get_source(server, file_path);
        if (source) {
          if (source->open_status == OPENED) {
            process_content(server, file_path, text);
          } else {
            log_error("Source not found");
            return;
          }
        } else {
          log_error("Source should be opened first");
          return;
        }
        print_sources(server->sources);
      }
    }
  } else {
    log_error("Couldn't parse content changes");
    return;
  }
}

void text_document_did_close(Server *server, Request *request) {
  log_info("Closing document...");
  const cJSON *text_document =
      cJSON_GetObjectItemCaseSensitive(request->params, "textDocument");

  char *uri;
  if (cJSON_IsObject(text_document)) {
    const cJSON *json_uri =
        cJSON_GetObjectItemCaseSensitive(text_document, "uri");
    if (cJSON_IsString(json_uri) && (json_uri->valuestring != NULL)) {
      uri = strdup(json_uri->valuestring);
    } else {
      log_error("Couldn't parse URI");
      return;
    }
  } else {
    log_error("Couldn't parse text document");
    return;
  }

  char *file_path = get_file_path(uri);
  Source *source = get_source(server, file_path);
  if (source) {
    source->open_status = CLOSED;
  } else {
    log_error("Source not found");
    return;
  }

  print_sources(server->sources);
}
