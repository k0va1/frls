#include <stdbool.h>

#ifndef UTILS_H_INCLUDED
#define UTILS_H_INCLUDED

char *trim(char *str);
void fail(char *msg);
void log_info(char *msg, ...);
void log_error(char *msg, ...);

int is_dir(char *file_path);
int is_file(char *file_path);
bool is_ends_with(char *left, char *right);
bool is_starts_with(char *left, char *right);
char *delete_prefix(char *str, char *prefix);
char *remove_leading_dashes(char *str);
char *get_file_path(char *uri);
char *readall(char *file_path);

#endif
