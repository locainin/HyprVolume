#ifndef HYPRVOLUME_CONFIG_JSON_TOKEN_SCAN_INTERNAL_H
#define HYPRVOLUME_CONFIG_JSON_TOKEN_SCAN_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

const char *osd_json_skip_whitespace(const char *text);
bool osd_json_is_value_terminator(char ch);
const char *osd_json_skip_string_token(const char *cursor);
const char *osd_json_skip_container_token(const char *cursor);
const char *osd_json_skip_value_token(const char *cursor);
bool osd_json_match_key_at_cursor(const char *cursor, const char *key, size_t key_len);

#endif
