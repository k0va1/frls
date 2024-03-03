#include "commands.h"
#include "ignore.h"
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
  source->file_path = strndup(file_path, strlen(file_path));
  source->content = strndup(content, strlen(content));
  arrput(server->sources, source);
  return source;
}

Source *get_source(Server *server, char *file_path) {
  Source *source = NULL;

  for (size_t i = 0; i < arrlen(server->sources); ++i) {
    if (strcmp(file_path, server->sources[i]->file_path) == 0) {
      source = server->sources[i];
      break;
    }
  }
  return source;
}

void sync_source(Server *server, char *file_path, char *content) {
  log_info("Syncing file %s...", file_path);

  Source *source = get_source(server, file_path);
  if (source) {
    free(source->content);
    source->content = strndup(content, strlen(content));
    log_info("Source updated");
  } else {
    source = add_source(server, file_path, content);
    source->open_status = OPENED;
    log_info("New source added");
  }
}

void process_file(Server *server, char *file_path) {
  if (is_ignored_file(file_path))
    return;

  log_info("Processing file `%s`", file_path);
  if (is_includes(SUPPORTED_FILE_EXTENSIONS, file_ext(file_path))) {
    char *content = readall(file_path);
    Source *source = add_source(server, file_path, content);
    free(content);
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
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0 ||
            is_ignored_dir(file_path)) {
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
    log_error("Error while reading `%s` because of error: '%s'", root_path, strerror(errno));
  }
}

void initialize(Server *server, Client *client, Request *request) {
  log_info("Initializing...");

  Config *config = server->config;

  const cJSON *process_id = cJSON_GetObjectItemCaseSensitive(request->params, "processId");
  if (cJSON_IsNumber(process_id)) {
    config->client_process_id = process_id->valueint;
  }
  const cJSON *client_info = cJSON_GetObjectItemCaseSensitive(request->params, "clientInfo");
  if (client_info != NULL) {
    const cJSON *client_name = cJSON_GetObjectItemCaseSensitive(client_info, "name");
    if (cJSON_IsString(client_name) && (client_name->valuestring != NULL)) {
      config->client_name = strdup(client_name->valuestring);
    }
    const cJSON *client_version = cJSON_GetObjectItemCaseSensitive(client_info, "version");
    if (cJSON_IsString(client_version) && (client_version->valuestring != NULL)) {
      config->client_version = strdup(client_version->valuestring);
    }
    const cJSON *root_uri = cJSON_GetObjectItemCaseSensitive(request->params, "rootUri");
    if (cJSON_IsString(root_uri) && (root_uri->valuestring != NULL)) {
      config->project_root = strdup(root_uri->valuestring);
    }

    const cJSON *capabilities = cJSON_GetObjectItemCaseSensitive(request->params, "capabilities");
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

  cJSON *result = cJSON_CreateObject();
  cJSON *response = cJSON_CreateObject();
  cJSON *server_info = cJSON_CreateObject();
  cJSON_AddItemToObject(server_info, "name", cJSON_CreateString(SERVER_NAME));
  cJSON_AddItemToObject(server_info, "version", cJSON_CreateString(FRLS_VERSION));
  cJSON_AddItemToObject(result, "serverInfo", server_info);

  cJSON *server_capabilities = cJSON_CreateObject();

  cJSON_AddItemToObject(result, "capabilities", server_capabilities);
  cJSON *text_document_sync = cJSON_CreateObject();

  cJSON_AddItemToObject(text_document_sync, "openClose", cJSON_CreateBool(true));
  // FULL
  // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocumentSyncKind
  cJSON_AddItemToObject(text_document_sync, "change", cJSON_CreateNumber(1));
  cJSON_AddItemToObject(server_capabilities, "textDocumentSync", cJSON_CreateNumber(1));
  cJSON_AddItemToObject(server_capabilities, "definitionProvider", cJSON_CreateBool(true));

  cJSON *req_id = cJSON_CreateNumber(request->id);
  cJSON_AddItemToObject(response, "id", req_id);
  cJSON_AddItemToObject(response, "result", result);

  char *json_str = cJSON_PrintUnformatted(response);
  send_response(client->socket, 200, json_str);
  log_info("Server initialized");

  print_config(server->config);
  print_sources(server->sources);
  cJSON_Delete(response);
}

void shutdown_server(Server *server, Client *client) {
  log_info("Shutting down");
  server->status = SHUTDOWN;
TODO:
  send_response(client->socket, 200, "");
}

void exit_server(Server *server) {
  log_info("Exiting");
  if (server->status == SHUTDOWN) {
    exit(0);
  } else {
    exit(1);
  }
}

void text_document_did_open(Server *server, Client *client, Request *request) {
  log_info("Opening document...");
  const cJSON *text_document = cJSON_GetObjectItemCaseSensitive(request->params, "textDocument");

  if (cJSON_IsObject(text_document)) {
    char *uri;
    char *language_id;
    char *text;

    const cJSON *json_uri = cJSON_GetObjectItemCaseSensitive(text_document, "uri");
    if (cJSON_IsString(json_uri) && (json_uri->valuestring != NULL)) {
      uri = strdup(json_uri->valuestring);
    } else {
      log_error("Couldn't parse URI");
      return;
    }

    const cJSON *json_language_id = cJSON_GetObjectItemCaseSensitive(text_document, "languageId");
    if (cJSON_IsString(json_language_id) && (json_language_id->valuestring != NULL)) {
      language_id = strdup(json_language_id->valuestring);
    } else {
      log_error("Couldn't parse language id");
      return;
    }

    const cJSON *json_text = cJSON_GetObjectItemCaseSensitive(text_document, "text");
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

void text_document_did_change(Server *server, Client *client, Request *request) {
  log_info("Changing document...");
  const cJSON *text_document = cJSON_GetObjectItemCaseSensitive(request->params, "textDocument");
  const cJSON *content_changes =
      cJSON_GetObjectItemCaseSensitive(request->params, "contentChanges");

  char *uri;
  if (cJSON_IsObject(text_document)) {
    const cJSON *json_uri = cJSON_GetObjectItemCaseSensitive(text_document, "uri");
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
  const cJSON *text_document = cJSON_GetObjectItemCaseSensitive(request->params, "textDocument");

  char *uri;
  if (cJSON_IsObject(text_document)) {
    const cJSON *json_uri = cJSON_GetObjectItemCaseSensitive(text_document, "uri");
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
    log_info("Source closed");
  } else {
    log_error("Source not found");
    return;
  }

  print_sources(server->sources);
}

void initialized(Server *server, Client *client) { log_info("Initialized"); }

// Go to definition
bool node_supports_go_to_definition(pm_node_t *node) {
  return PM_NODE_TYPE_P(node, PM_CONSTANT_READ_NODE);
}

pm_node_t *get_node_by_position(Source *source, size_t line, size_t character) {
  line++; // prism lines indexed by 1

  VisitArgs *args = malloc(sizeof(VisitArgs));
  args->found_node = NULL;
  args->line = line;
  args->character = character;
  traverse_ast(source->root, source->parser, find_node_by_location, args);

  return args->found_node;
}

Location *get_locations_by_position(Server *server, Source *source, size_t line, size_t character) {
  Location *locs = NULL;

  pm_node_t *node = get_node_by_position(source, line, character);

  if (node != NULL && node_supports_go_to_definition(node)) {
    switch (PM_NODE_TYPE(node)) {
    case PM_CONSTANT_READ_NODE: {
      pm_constant_read_node_t *cast = (pm_constant_read_node_t *)node;
      pm_line_column_t start = pm_newline_list_line_column(
          &source->parser->newline_list, node->location.start, source->parser->start_line);
      pm_constant_t *constant =
          pm_constant_pool_id_to_constant(&source->parser->constant_pool, cast->name);

      char *node_name = strndup((char *)constant->start, constant->length);
      for (int i = 0; i < hmlen(server->parsed_info->consts); i++) {
        if (strcmp(server->parsed_info->consts[i].value->const_name, node_name) == 0) {
          locs = server->parsed_info->consts[i].value->locations;
        }
      }
    }
    default: {
      break;
    }
    }
  } else {
    log_info("Node not found");
  }

  return locs;
}

void go_to_definition(Server *server, Client *client, Request *request) {
  log_info("Going to definition...");

  const cJSON *text_document = cJSON_GetObjectItemCaseSensitive(request->params, "textDocument");
  const cJSON *position = cJSON_GetObjectItemCaseSensitive(request->params, "position");

  if (!(cJSON_IsObject(text_document) && cJSON_IsObject(position))) {
    invalid_params(client, request);
  }

  cJSON *result = cJSON_CreateObject();
  cJSON *response = cJSON_CreateObject();

  char *uri;
  size_t line;
  size_t character;

  const cJSON *json_uri = cJSON_GetObjectItemCaseSensitive(text_document, "uri");
  if (cJSON_IsString(json_uri) && (json_uri->valuestring != NULL)) {
    uri = strdup(json_uri->valuestring);
  } else {
    log_error("Couldn't parse URI");
    return;
  }

  const cJSON *json_line = cJSON_GetObjectItemCaseSensitive(position, "line");
  if (cJSON_IsNumber(json_line)) {
    line = json_line->valueint;
  } else {
    log_error("Couldn't parse line");
    return;
  }

  const cJSON *json_character = cJSON_GetObjectItemCaseSensitive(position, "character");
  if (cJSON_IsNumber(json_character)) {
    character = json_character->valueint;
  } else {
    log_error("Couldn't parse character");
    return;
  }

  char *file_path = get_file_path(uri);
  Source *source = get_source(server, file_path);

  if (source) {
    Location *locations = get_locations_by_position(server, source, line, character);

    cJSON *req_id = cJSON_CreateNumber(request->id);
    cJSON_AddItemToObject(response, "id", req_id);

    if (arrlen(locations) == 0) {
      cJSON_AddItemToObject(response, "result", cJSON_CreateNull());
    } else if (arrlen(locations) == 1) {
      char *uri = build_uri(locations->file_path);
      cJSON *json_uri = cJSON_CreateString(uri);
      cJSON_AddItemToObject(result, "uri", json_uri);
      free(uri);

      cJSON *range = cJSON_CreateObject();

      cJSON *start = cJSON_CreateObject();
      cJSON *start_line = cJSON_CreateNumber(locations->start->line);
      cJSON *start_character = cJSON_CreateNumber(locations->start->character);
      cJSON_AddItemToObject(start, "line", start_line);
      cJSON_AddItemToObject(start, "character", start_character);
      cJSON_AddItemToObject(range, "start", start);

      cJSON *end = cJSON_CreateObject();
      cJSON *end_line = cJSON_CreateNumber(locations->end->line);
      cJSON *end_character = cJSON_CreateNumber(locations->end->character);
      cJSON_AddItemToObject(end, "line", end_line);
      cJSON_AddItemToObject(end, "character", end_character);
      cJSON_AddItemToObject(range, "end", end);

      cJSON_AddItemToObject(result, "range", range);

      cJSON_AddItemToObject(response, "result", result);
    } else {
      cJSON *locs = cJSON_CreateArray();
      for (size_t i = 0; i < arrlen(locations); ++i) {
        cJSON *loc = cJSON_CreateObject();

        char *uri = build_uri(locations[i].file_path);
        cJSON *json_uri = cJSON_CreateString(uri);
        cJSON_AddItemToObject(loc, "uri", json_uri);
        free(uri);

        cJSON *range = cJSON_CreateObject();

        cJSON *start = cJSON_CreateObject();
        cJSON *start_line = cJSON_CreateNumber(locations[i].start->line);
        cJSON *start_character = cJSON_CreateNumber(locations[i].start->character);
        cJSON_AddItemToObject(start, "line", start_line);
        cJSON_AddItemToObject(start, "character", start_character);
        cJSON_AddItemToObject(range, "start", start);

        cJSON *end = cJSON_CreateObject();
        cJSON *end_line = cJSON_CreateNumber(locations[i].end->line);
        cJSON *end_character = cJSON_CreateNumber(locations[i].end->character);
        cJSON_AddItemToObject(end, "line", end_line);
        cJSON_AddItemToObject(end, "character", end_character);
        cJSON_AddItemToObject(range, "end", end);

        cJSON_AddItemToObject(loc, "range", range);

        cJSON_AddItemToArray(locs, loc);
      }
      cJSON_AddItemToObject(response, "result", locs);
    }

    char *json_str = cJSON_PrintUnformatted(response);
    send_response(client->socket, 200, json_str);
  } else {
    log_info("Source not found");
  }
}
