#pragma once
#include <stddef.h>

typedef struct {
    int read_fd, write_fd;
    int read_offset, write_offset;
    size_t read_buf_sz;
    char *read_buf;
} unix_xpc_io_ctx_t;

unix_xpc_io_ctx_t *create_unix_io_ctx(size_t buf_sz, int ifd, int ofd);
int unix_xpc_read(void *io_ctx, char **buffer, int offset, size_t bytes_max);
int unix_xpc_write(void *io_ctx, char **buffer, int offset, size_t bytes_max);
void unix_xpc_reset(void *io_ctx, int which, size_t bytes);
void unix_xpc_io_ctx_free(unix_xpc_io_ctx_t *ctx);
