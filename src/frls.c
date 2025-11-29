#include "config.h"
#include "server.h"

int main(int argc, char *argv[]) {
  Config *config = create_config(argc, argv);
  Server *server = create_server(config);
  start_server(server);
  destroy_server(server);
  destroy_config(config);

  return 0;
}
