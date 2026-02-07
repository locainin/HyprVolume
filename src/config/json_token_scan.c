#include "config/json_token_scan_internal.h"

#include <ctype.h>
#include <string.h>

/* Skips whitespace between JSON tokens. */
const char *osd_json_skip_whitespace(const char *text) {
    while (text != NULL && *text != '\0' && isspace((unsigned char)*text) != 0) {
        /* Parser consumers rely on the first non-space byte. */
        text++;
    }
    return text;
}

/* Recognizes a legal boundary after a primitive JSON token. */
bool osd_json_is_value_terminator(char ch) {
    return ch == ',' || ch == '}' || ch == ']' || ch == '\0';
}

/* Skips one JSON string token and stops after the closing quote. */
const char *osd_json_skip_string_token(const char *cursor) {
    bool escaped = false;

    if (cursor == NULL || *cursor != '"') {
        return NULL;
    }

    cursor++;
    while (*cursor != '\0') {
        if (!escaped && *cursor == '\\') {
            /* Backslash escapes exactly one following character in JSON. */
            escaped = true;
            cursor++;
            continue;
        }

        if (!escaped && *cursor == '"') {
            /* Closing quote ends the current token. */
            return cursor + 1;
        }

        /* Escape state is one-character wide and resets immediately. */
        escaped = false;
        cursor++;
    }

    return NULL;
}

/*
 * Skips one object or array token including nested structures.
 * Strings are tracked so braces inside string literals do not affect depth.
 */
const char *osd_json_skip_container_token(const char *cursor) {
    int brace_depth = 0;
    int bracket_depth = 0;
    bool in_string = false;
    bool escaped = false;

    if (cursor == NULL) {
        return NULL;
    }
    if (*cursor != '{' && *cursor != '[') {
        return NULL;
    }

    for (; *cursor != '\0'; cursor++) {
        if (in_string) {
            if (!escaped && *cursor == '\\') {
                /* Escaped characters are ignored for structural parsing. */
                escaped = true;
                continue;
            }
            if (!escaped && *cursor == '"') {
                /* Exit string mode once unescaped quote is observed. */
                in_string = false;
            }
            escaped = false;
            continue;
        }

        if (*cursor == '"') {
            /* String mode shields braces/brackets from depth counters. */
            in_string = true;
            escaped = false;
            continue;
        }
        if (*cursor == '{') {
            /* Count nested objects to find the matching close brace. */
            brace_depth++;
            continue;
        }
        if (*cursor == '[') {
            /* Arrays share the same balanced-container tracking. */
            bracket_depth++;
            continue;
        }
        if (*cursor == '}') {
            brace_depth--;
            if (brace_depth < 0) {
                /* Negative depth means malformed container input. */
                return NULL;
            }
        } else if (*cursor == ']') {
            bracket_depth--;
            if (bracket_depth < 0) {
                /* Negative depth means malformed container input. */
                return NULL;
            }
        }

        if (brace_depth == 0 && bracket_depth == 0) {
            /* Both depths zero indicates container end was reached. */
            return cursor + 1;
        }
    }

    return NULL;
}

/* Skips any JSON value token and returns the position after that token. */
const char *osd_json_skip_value_token(const char *cursor) {
    const char *value_start = osd_json_skip_whitespace(cursor);
    const char *value_end = NULL;

    if (value_start == NULL || *value_start == '\0') {
        return NULL;
    }

    if (*value_start == '"') {
        return osd_json_skip_string_token(value_start);
    }
    if (*value_start == '{' || *value_start == '[') {
        return osd_json_skip_container_token(value_start);
    }

    value_end = value_start;
    while (*value_end != '\0' && !osd_json_is_value_terminator(*value_end)) {
        /* Primitive tokens extend until a JSON structural terminator. */
        value_end++;
    }

    if (value_end == value_start) {
        return NULL;
    }

    return value_end;
}

/* Checks a key token and verifies the required ':' separator. */
bool osd_json_match_key_at_cursor(const char *cursor, const char *key, size_t key_len) {
    const char *after_key = NULL;

    /* Key text must match exactly, including case and punctuation. */
    if (strncmp(cursor, key, key_len) != 0) {
        return false;
    }

    after_key = cursor + key_len;
    if (*after_key != '"') {
        /* Enforces full-key match rather than prefix-key match. */
        return false;
    }

    /* Top-level object syntax requires a colon after key token. */
    after_key = osd_json_skip_whitespace(after_key + 1);
    return after_key != NULL && *after_key == ':';
}
