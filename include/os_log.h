#pragma once
#include <alibc/containers/array.h>

typedef struct {
    array_t *strlens;
    array_t *log_strings;
} log_info_t;

log_info_t *initialize_log_info(char *index_fname, char *strings_fname);
