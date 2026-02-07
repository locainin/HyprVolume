#ifndef HYPRVOLUME_CONFIG_JSON_CONFIG_FIELDS_H
#define HYPRVOLUME_CONFIG_JSON_CONFIG_FIELDS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

const char *osd_config_find_key(const char *json_text, const char *key);
bool osd_config_validate_json_envelope(const char *json_text, FILE *err_stream);
bool osd_config_validate_top_level_keys(
    const char *json_text,
    const char *const *allowed_keys,
    size_t allowed_key_count,
    FILE *err_stream
);
bool osd_config_parse_long_value(const char *json_text, const char *key, long *out_value);
bool osd_config_parse_bool_value(const char *json_text, const char *key, bool *out_value);
bool osd_config_parse_string_value(const char *json_text, const char *key, char *out_buffer, size_t out_size);

#endif
