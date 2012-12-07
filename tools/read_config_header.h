#ifndef CCAN_TOOLS_READ_CONFIG_HEADER_H
#define CCAN_TOOLS_READ_CONFIG_HEADER_H
#include <stdbool.h>

/* Get token if it's equal to token. */
bool get_token(const char **line, const char *token);

/* Get an identifier token. */
char *get_symbol_token(void *ctx, const char **line);

/* Read config header from config_dir/config.h: sets compiler/cflags. */
char *read_config_header(const char *config_dir, bool verbose);

#endif /* CCAN_TOOLS_READ_CONFIG_HEADER_H */
