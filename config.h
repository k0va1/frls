#include <sys/types.h>
#include <stdbool.h>

#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED

// CLI stuff
#define FRLS_VERSION "0.0.1"
#define HELP_MESSAGE                                                           \
  "\n\
OVERVIEW: Fast Ruby Language Server\n\
\n\
See more on https://github.com/k0va1/frls\n\
USAGE: frls [options]\n\
\n\
OPTIONS:\n\
  --help, -h            - Display available options\n\
  --version, -v         - Display the version of this program\n\
  --host                - Specify host(default: 127.0.0.1)\n\
  --port                - Specify port(default: 1488)\n\
"
#define HOST "127.0.0.1"
#define PORT 1488

typedef struct {
  char *host;
  char *project_root;
  bool is_initialized;
  uint port;
} Config;

Config *create_config(int argc, char *argv[]);
void destroy_config(Config *config);

#endif
