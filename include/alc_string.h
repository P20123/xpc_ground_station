#pragma once
#include <alibc/containers/dynabuf.h>
typedef struct {
    dynabuf_t *str;
    int size;
} string_t;

string_t *create_string(string_t *loc);

void string_append_cstr(string_t *str, char *buf);

void string_append_buf(string_t *str, char *buf, int len);

char *string_get_cstr(string_t *str);

char *string_reserve(string_t *str, int size);
