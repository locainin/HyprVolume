#include "config/json_config_fields.h"
#include "config/json_token_scan_internal.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* Parses an integer key with strict token termination checks. */
bool osd_config_parse_long_value(const char *json_text, const char *key, long *out_value) {
    const char *value_start = osd_config_find_key(json_text, key);
    const char *token_end = NULL;
    char *end_ptr = NULL;

    if (value_start == NULL || out_value == NULL) {
        return false;
    }

    errno = 0;
    /* Base-10 parse only; locale-independent integer grammar is expected. */
    *out_value = strtol(osd_json_skip_whitespace(value_start), &end_ptr, 10);
    if (errno != 0 || end_ptr == NULL) {
        return false;
    }
    if (end_ptr == osd_json_skip_whitespace(value_start)) {
        return false;
    }

    /* Value token must end at a JSON separator. */
    token_end = osd_json_skip_whitespace(end_ptr);
    return token_end != NULL && osd_json_is_value_terminator(*token_end);
}

/* Parses a boolean key and validates that trailing bytes are separators. */
bool osd_config_parse_bool_value(const char *json_text, const char *key, bool *out_value) {
    const char *value_start = osd_json_skip_whitespace(osd_config_find_key(json_text, key));
    const char *token_end = NULL;

    if (value_start == NULL || out_value == NULL) {
        return false;
    }

    if (strncmp(value_start, "true", 4) == 0) {
        /* Boolean token requires separator after the literal text. */
        token_end = osd_json_skip_whitespace(value_start + 4);
        if (token_end == NULL || !osd_json_is_value_terminator(*token_end)) {
            return false;
        }
        *out_value = true;
        return true;
    }

    if (strncmp(value_start, "false", 5) == 0) {
        /* Boolean token requires separator after the literal text. */
        token_end = osd_json_skip_whitespace(value_start + 5);
        if (token_end == NULL || !osd_json_is_value_terminator(*token_end)) {
            return false;
        }
        *out_value = false;
        return true;
    }

    return false;
}

/*
 * Parses a quoted JSON string value.
 * Escape sequences are preserved as raw bytes because CSS/path consumers parse
 * plain strings and this parser only validates boundaries and size.
 */
bool osd_config_parse_string_value(const char *json_text, const char *key, char *out_buffer, size_t out_size) {
    const char *value_start = osd_json_skip_whitespace(osd_config_find_key(json_text, key));
    const char *token_end = NULL;
    size_t copy_len = 0U;
    bool escaped = false;

    if (value_start == NULL || out_buffer == NULL || out_size == 0U || *value_start != '"') {
        return false;
    }

    /* Skip opening quote before scanning content bytes. */
    value_start++;
    while (value_start[copy_len] != '\0') {
        if (!escaped && value_start[copy_len] == '\\') {
            /* Escape applies to the next byte only. */
            escaped = true;
            copy_len++;
            continue;
        }

        if (!escaped && value_start[copy_len] == '"') {
            /* Closing quote found; copy length is finalized. */
            break;
        }

        /* Escape flag resets after consuming one following byte. */
        escaped = false;
        copy_len++;
    }

    if (value_start[copy_len] != '"' || copy_len >= out_size) {
        return false;
    }

    /* Ensure no unexpected trailing content after closing quote. */
    token_end = osd_json_skip_whitespace(value_start + copy_len + 1U);
    if (token_end == NULL || !osd_json_is_value_terminator(*token_end)) {
        return false;
    }

    /*
     * Copy one byte at a time to keep bounds reasoning explicit for static
     * analysis and avoid bulk-memory API warnings in hardened lint profiles.
     */
    for (size_t index = 0U; index < copy_len; index++) {
        out_buffer[index] = value_start[index];
    }
    out_buffer[copy_len] = '\0';
    return true;
}
