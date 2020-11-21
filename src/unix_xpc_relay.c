#include <unix_xpc_relay.h>
#include <stdlib.h>
#include <unistd.h>
#include <tinyxpc/tinyxpc.h>
#include <tinyxpc/xpc_relay.h>

unix_xpc_io_ctx_t *create_unix_io_ctx(size_t buf_sz, int ifd, int ofd) {
    unix_xpc_io_ctx_t *r = NULL;
    r = malloc(sizeof(unix_xpc_io_ctx_t));
    if(r == NULL) {
        goto done;
    }
    r->read_buf = malloc(buf_sz);
    if(r->read_buf == NULL) {
        free(r);
        goto done;
    }
    r->read_buf_sz = buf_sz;
    r->read_offset = 0;
    r->write_offset = 0;
    r->read_fd = ifd;
    r->write_fd = ofd;
done:
    return r;
}

int unix_xpc_read(void *io_ctx, char **buffer, int offset, size_t bytes_max) {
    unix_xpc_io_ctx_t *ctx = (unix_xpc_io_ctx_t*)io_ctx;
    int bytes = 0;
    // read into the specified location if read_buf is set, otherwise store it
    // in our own context.
    if(*buffer == NULL) {
        bytes = read(ctx->read_fd, ctx->read_buf + offset, bytes_max);
        // tell the relay where we read into
        *buffer = (char*)ctx->read_buf;
    }
    else {
        bytes = read(ctx->read_fd, *buffer + offset, bytes_max);
    }
    if(bytes > 0) {
        ctx->read_offset += bytes;
    }
    else {
        bytes = 0;
    }
    return bytes > 0 ? bytes:0;
}

int unix_xpc_write(void *io_ctx, char **buffer, int offset, size_t bytes_max) {
    unix_xpc_io_ctx_t *ctx = (unix_xpc_io_ctx_t*)io_ctx;
    int bytes = write(ctx->write_fd, *buffer + offset, bytes_max);
    if(bytes > 0) {
        ctx->write_offset += bytes;
    }
    else {
        // something went wrong. FIXME we have no way to say "write failed"...
        // should we pass this through to the relay?
        bytes = 0;
    }
    return bytes;
}

void unix_xpc_reset(void *io_ctx, int which, size_t bytes) {
    unix_xpc_io_ctx_t *ctx = (unix_xpc_io_ctx_t*)io_ctx;
    // we don't handle more than one message at a time in this impl,
    // so we can just reset to zero bytes offset.
    if(!which) {
        if(bytes == -1) {
            // discard all data
            int check_bytes = bytes == -1 ? ctx->read_buf_sz:bytes;
            bool sentinel = false;
            while(sentinel) {
                int bytes_read = read(ctx->read_fd, ctx->read_buf, check_bytes);
                if(bytes_read < check_bytes) break;
            }
            ctx->read_offset = 0;
        }
        else {
            size_t bytes_read = 0;
            while(bytes_read < bytes) {
                bytes_read = read(ctx->read_fd, ctx->read_buf, bytes);
            }
            ctx->read_offset -=bytes;
        }
    }
    else {
        ctx->write_offset = 0;
    }
}
void unix_xpc_io_ctx_free(unix_xpc_io_ctx_t *ctx) {
    free(ctx->read_buf);
    free(ctx);
}
