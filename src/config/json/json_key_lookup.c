#include "config/json/json_config_fields.h"
#include "config/json/json_token_scan_internal.h"

#include <string.h>

/* Finds a top-level key token and returns the first character of its value. */
const char *osd_config_find_key(const char *json_text, const char *key) {
    size_t key_len = 0U;
    const char *cursor = NULL;
    int brace_depth = 0;
    int bracket_depth = 0;
    bool in_string = false;
    bool escaped = false;

    if (json_text == NULL || key == NULL || *key == '\0') {
        return NULL;
    }
    key_len = strlen(key);

    cursor = json_text;
    while (*cursor != '\0') {
        if (in_string) {
            if (!escaped && *cursor == '\\') {
                /* Escaped content in strings does not affect parse depth. */
                escaped = true;
                cursor++;
                continue;
            }

            if (!escaped && *cursor == '"') {
                /* End of current string token. */
                in_string = false;
            }
            escaped = false;
            cursor++;
            continue;
        }

        if (*cursor == '"') {
            const char *candidate = cursor + 1;

            /*
             * Only top-level object keys are considered. Nested keys are ignored
             * on purpose because config parsing is defined for top-level fields.
             */
            if (brace_depth == 1 && bracket_depth == 0 && osd_json_match_key_at_cursor(candidate, key, key_len)) {
                const char *after_key = osd_json_skip_whitespace(candidate + key_len + 1);
                /* Return pointer to first value byte after ':'. */
                return (after_key == NULL) ? NULL : after_key + 1;
            }

            /* Enter string mode for non-matching string tokens. */
            in_string = true;
            escaped = false;
            cursor++;
            continue;
        }

        if (*cursor == '{') {
            /* Track nested object depth to isolate top-level keys. */
            brace_depth++;
            cursor++;
            continue;
        }
        if (*cursor == '}') {
            if (brace_depth > 0) {
                /* Defensive guard avoids negative depth on malformed input. */
                brace_depth--;
            }
            cursor++;
            continue;
        }
        if (*cursor == '[') {
            /* Track nested arrays to exclude keys inside array objects. */
            bracket_depth++;
            cursor++;
            continue;
        }
        if (*cursor == ']') {
            if (bracket_depth > 0) {
                /* Defensive guard avoids negative depth on malformed input. */
                bracket_depth--;
            }
            cursor++;
            continue;
        }

        cursor++;
    }

    return NULL;
}
