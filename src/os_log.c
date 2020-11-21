#include <os_log.h>
#include <alibc/containers/array.h>
#include <alibc/containers/iterator.h>
#include <alibc/containers/array_iterator.h>
#include <stdio.h>
#include <alc_string.h>

#define DB_VAL(X) (*(void **)&X)
#define DB_PTR(X) ((void*)X)

log_info_t *initialize_log_info(char *index_fname, char *strings_fname) {
    int idx_rd_size = 0;
    int16_t msg_id = 0;
    int16_t msg_offsets[2] = {0};
    int count = 0;

    log_info_t *r = malloc(sizeof(log_info_t));
    if(r == NULL) {
        goto done;
    }
    r->strlens = create_array(1, sizeof(int));
    if(r->strlens == NULL) {
        goto free_structure;
    }
    r->log_strings = create_array(1, sizeof(string_t));
    if(r->log_strings == NULL) {
        goto free_strlens;
    }
    FILE *index = fopen(index_fname, "rb");
    if(index == NULL) {
        goto free_log_strs;
    }
    FILE *strings = fopen(strings_fname, "rb");
    if(strings == NULL) {
        goto close_index;
    }

    while((idx_rd_size = fread(&msg_id, sizeof(int16_t), 1, index)) > 0) {
        if(count % 2 > 0) {
            int len = msg_offsets[1] - msg_offsets[0];
            // roll buffer back by one
            msg_offsets[0] = msg_offsets[1];
            array_append(r->strlens, DB_VAL(len));
            if(array_status(r->strlens) != ALC_ARRAY_SUCCESS) {
                // uh oh
            }
        }
        msg_offsets[1] = msg_id;
        count++;
    }

    iter_context *it = create_array_iterator(r->strlens);
    for(int *next = iter_next(it); next; next = iter_next(it)) {
        int len = *next;
        string_t str;
        string_t *s = create_string(&str);
        if(s == NULL) {
            goto close_strings;
        }
        char *str_end = string_reserve(&str, len);
        fread(str_end, sizeof(char), len, strings);
         // this could break if the strings are not in order in the index.
        array_append(r->log_strings, &str);
        if(array_status(r->log_strings) != ALC_ARRAY_SUCCESS) {
            // uh oh
        }
    }
    iter_free(it);
    // not the cleanest, whatever
    fclose(strings);
    fclose(index);
    goto done;
close_strings:
    fclose(strings);
close_index:
    fclose(index);
free_log_strs:
    array_free(r->log_strings);
free_strlens:
    array_free(r->strlens);
free_structure:
    free(r);
    r = NULL;
done:
    return r;
}
