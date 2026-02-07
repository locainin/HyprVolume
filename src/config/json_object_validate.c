#include "config/json_config_fields.h"
#include "config/json_token_scan_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Writes a fixed parser error message. */
static void osd_config_write_error_literal(FILE *err_stream, const char *message) {
    if (err_stream == NULL || message == NULL) {
        return;
    }
    /* Fixed-string path avoids formatter complexity in error handling. */
    (void)fputs(message, err_stream);
}

/* Writes an error message formed as prefix + key + suffix. */
static void osd_config_write_error_key(FILE *err_stream, const char *prefix, const char *key, const char *suffix) {
    size_t key_length = 0U;

    if (err_stream == NULL || prefix == NULL || key == NULL || suffix == NULL) {
        return;
    }

    key_length = strlen(key);
    /* Prefix/suffix are fixed literals owned by call sites. */
    (void)fputs(prefix, err_stream);
    if (key_length > 0U) {
        /* Raw key write avoids temporary format buffers. */
        (void)fwrite(key, 1U, key_length, err_stream);
    }
    (void)fputs(suffix, err_stream);
}

/*
 * Validates high-level JSON structure constraints required by this project:
 * top-level object, balanced delimiters, valid string boundaries, and
 * no trailing non-whitespace content after the object.
 */
bool osd_config_validate_json_envelope(const char *json_text, FILE *err_stream) {
    const char *cursor = NULL;
    size_t text_len = 0U;
    size_t trailing_index = 0U;
    int brace_depth = 0;
    int bracket_depth = 0;
    bool in_string = false;
    bool escaped = false;

    if (json_text == NULL || err_stream == NULL) {
        return false;
    }

    cursor = osd_json_skip_whitespace(json_text);
    if (cursor == NULL || *cursor != '{') {
        osd_config_write_error_literal(err_stream, "Config file must be a JSON object\n");
        return false;
    }

    for (; *cursor != '\0'; cursor++) {
        if (in_string) {
            if (!escaped && *cursor == '\\') {
                /* Escaped bytes are ignored by structural delimiters. */
                escaped = true;
                continue;
            }
            if (!escaped && *cursor == '"') {
                /* Unescaped quote exits string mode. */
                in_string = false;
            }
            escaped = false;
            continue;
        }

        if (*cursor == '"') {
            /* String content must not affect delimiter depth checks. */
            in_string = true;
            escaped = false;
            continue;
        }
        if (*cursor == '{') {
            /* Track nested objects for full envelope balance validation. */
            brace_depth++;
            continue;
        }
        if (*cursor == '[') {
            /* Arrays are tracked separately from object braces. */
            bracket_depth++;
            continue;
        }
        if (*cursor == '}') {
            brace_depth--;
            if (brace_depth < 0) {
                osd_config_write_error_literal(err_stream, "Config file has unmatched '}'\n");
                return false;
            }
            continue;
        }
        if (*cursor == ']') {
            bracket_depth--;
            if (bracket_depth < 0) {
                osd_config_write_error_literal(err_stream, "Config file has unmatched ']'\n");
                return false;
            }
            continue;
        }
    }

    if (in_string || brace_depth != 0 || bracket_depth != 0) {
        osd_config_write_error_literal(err_stream, "Config file is malformed JSON\n");
        return false;
    }

    text_len = strlen(json_text);
    trailing_index = text_len;
    while (trailing_index > 0U && isspace((unsigned char)json_text[trailing_index - 1U]) != 0) {
        trailing_index--;
    }
    if (trailing_index == 0U || json_text[trailing_index - 1U] != '}') {
        osd_config_write_error_literal(err_stream, "Config file must end with a JSON object\n");
        return false;
    }

    return true;
}

/*
 * Validates only top-level keys against an allowlist.
 * Duplicate keys and trailing commas are rejected explicitly.
 */
bool osd_config_validate_top_level_keys(
    const char *json_text,
    const char *const *allowed_keys,
    size_t allowed_key_count,
    FILE *err_stream
) {
    const char *cursor = NULL;
    bool *seen = NULL;
    bool ok = true;

    if (json_text == NULL || allowed_keys == NULL || err_stream == NULL) {
        return false;
    }
    if (allowed_key_count == 0U) {
        return true;
    }

    seen = calloc(allowed_key_count, sizeof(*seen));
    if (seen == NULL) {
        osd_config_write_error_literal(err_stream, "Failed to allocate key-validation state\n");
        return false;
    }

    cursor = osd_json_skip_whitespace(json_text);
    if (cursor == NULL || *cursor != '{') {
        free(seen);
        return false;
    }

    cursor++;
    /* Parse strict top-level key/value pairs one item at a time. */
    while (ok) {
        char key_buffer[128];
        size_t key_length = 0U;
        bool escaped = false;
        size_t key_index = 0U;
        bool key_found = false;

        cursor = osd_json_skip_whitespace(cursor);
        if (cursor == NULL) {
            /* Unexpected end-of-buffer is treated as malformed JSON. */
            ok = false;
            break;
        }
        if (*cursor == '}') {
            /* Empty object or fully-consumed object is valid termination. */
            break;
        }
        if (*cursor != '"') {
            osd_config_write_error_literal(err_stream, "Config object key must be a JSON string\n");
            ok = false;
            break;
        }

        cursor++;
        while (*cursor != '\0') {
            if (!escaped && *cursor == '\\') {
                /* Escaped key bytes are copied as raw bytes. */
                escaped = true;
                cursor++;
                continue;
            }
            if (!escaped && *cursor == '"') {
                /* Key string closes here. */
                break;
            }
            if (key_length + 1U >= sizeof(key_buffer)) {
                osd_config_write_error_literal(err_stream, "Config key exceeds supported length\n");
                ok = false;
                break;
            }

            /* Keep raw key bytes for exact allowlist comparison. */
            key_buffer[key_length] = *cursor;
            key_length++;
            escaped = false;
            cursor++;
        }
        if (!ok) {
            break;
        }
        if (*cursor != '"') {
            osd_config_write_error_literal(err_stream, "Config key string is not terminated\n");
            ok = false;
            break;
        }
        key_buffer[key_length] = '\0';
        cursor++;

        cursor = osd_json_skip_whitespace(cursor);
        if (cursor == NULL || *cursor != ':') {
            osd_config_write_error_key(err_stream, "Config key '", key_buffer, "' is missing ':' separator\n");
            ok = false;
            break;
        }
        /* Move to the value token start for this key. */
        cursor++;

        for (key_index = 0U; key_index < allowed_key_count; key_index++) {
            if (strcmp(key_buffer, allowed_keys[key_index]) == 0) {
                key_found = true;
                if (seen[key_index]) {
                    /* Duplicate key ambiguity is rejected by policy. */
                    osd_config_write_error_key(err_stream, "Duplicate config key '", key_buffer, "' is not allowed\n");
                    ok = false;
                } else {
                    /* Key is now marked to block duplicate occurrences. */
                    seen[key_index] = true;
                }
                break;
            }
        }
        if (!ok) {
            break;
        }
        if (!key_found) {
            osd_config_write_error_key(err_stream, "Unknown config key '", key_buffer, "'\n");
            ok = false;
            break;
        }

        cursor = osd_json_skip_value_token(cursor);
        if (cursor == NULL) {
            osd_config_write_error_key(err_stream, "Config key '", key_buffer, "' has an invalid JSON value\n");
            ok = false;
            break;
        }

        cursor = osd_json_skip_whitespace(cursor);
        if (cursor == NULL) {
            ok = false;
            break;
        }
        if (*cursor == ',') {
            /* Comma means another key/value pair should follow. */
            cursor++;
            cursor = osd_json_skip_whitespace(cursor);
            if (cursor == NULL || *cursor == '\0') {
                ok = false;
                break;
            }
            if (*cursor == '}') {
                /* Trailing comma after last key is rejected explicitly. */
                osd_config_write_error_literal(err_stream, "Config object has a trailing comma\n");
                ok = false;
                break;
            }
            continue;
        }
        if (*cursor == '}') {
            /* Clean end of object after current key/value pair. */
            break;
        }

        osd_config_write_error_key(err_stream, "Config object key '", key_buffer, "' is followed by invalid separator\n");
        ok = false;
        break;
    }

    free(seen);
    return ok;
}
