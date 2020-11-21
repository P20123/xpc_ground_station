#include <string.h>
#include <alibc/containers/dynabuf.h>
#include <stdlib.h>
#include <alc_string.h>

string_t *create_string(string_t *loc) {
    string_t *r = NULL;
    if(loc == NULL) {
        r = malloc(sizeof(string_t));
        if(r == NULL) {
            goto done;
        }
    }
    else {
        r = loc;
    }
    r->str = create_dynabuf(1, sizeof(char));
    if(r->str == NULL) {
        free(r);
        r = NULL;
        goto done;
    }
    r->size = 0;
done:
    return r;
}

void string_append_cstr(string_t *str, char *buf) {
    int len = strlen(buf);
    if(str->str->capacity < str->size + len) {
        dynabuf_resize(str->str, str->size + len);
        memset((char*)str->str->buf + str->size, 0, len);
    }
    memcpy((char*)str->str->buf + str->size, buf, len);
    str->size += len;
}

void string_append_buf(string_t *str, char *buf, int len) {
    if(str->str->capacity < str->size + len) {
        dynabuf_resize(str->str, str->size + len);
        memset((char*)str->str->buf + str->size, 0, len);
    }
    memcpy((char*)str->str->buf + str->size, buf, len);
    str->size += len;
}

char *string_get_cstr(string_t *str) {
    return (char*)str->str->buf;
}

char *string_reserve(string_t *str, int size) {
    if(size > str->size) {
        dynabuf_resize(str->str, str->size + size);
        memset((char*)str->str->buf + str->size, 0, size);
    }
    return (char*)str->str->buf + str->size;
}
