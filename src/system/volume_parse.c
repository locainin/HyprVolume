#include "system/volume_parse.h"

#include <ctype.h>
#include <math.h>
#include <string.h>

// Keeps UI percent outputs inside supported range
static int clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) {
        return min_value;
    }

    if (value > max_value) {
        return max_value;
    }

    return value;
}

// Advances over ASCII whitespace for tolerant parsing
static const char *skip_whitespace(const char *text) {
    while (text != NULL && *text != '\0' && isspace((unsigned char)*text) != 0) {
        text++;
    }
    return text;
}

// Writes parse status when caller provided storage
static void set_status(OSDVolumeParseStatus *status, OSDVolumeParseError error) {
    if (status == NULL) {
        return;
    }

    status->error = error;
}

// Parses an ASCII decimal token using dot as separator only
static bool parse_ascii_decimal_token(const char *text, const char **out_end, double *out_value) {
    size_t index = 0U;
    bool saw_digit = false;
    bool in_fraction = false;
    double value = 0.0;
    double fraction_scale = 0.1;

    if (text == NULL || out_end == NULL || out_value == NULL) {
        return false;
    }

    // Leading plus is tolerated to match strtod-style inputs
    if (text[index] == '+') {
        index++;
    }

    // Negative values are rejected at grammar level
    if (text[index] == '-') {
        return false;
    }

    // Parse integral and optional fractional section into a double
    while (text[index] != '\0') {
        unsigned char current = (unsigned char)text[index];

        if (current == '.') {
            if (in_fraction) {
                break;
            }
            in_fraction = true;
            index++;
            continue;
        }

        if (!isdigit(current)) {
            break;
        }

        saw_digit = true;
        if (!in_fraction) {
            value = (value * 10.0) + (double)(current - (unsigned char)'0');
        } else {
            value += (double)(current - (unsigned char)'0') * fraction_scale;
            fraction_scale *= 0.1;
        }

        index++;
    }

    if (!saw_digit || !isfinite(value)) {
        return false;
    }

    // Return both parsed numeric value and cursor position after token
    *out_end = text + index;
    *out_value = value;
    return true;
}

// Parses wpctl output while tolerating known output variants
bool osd_volume_parse_wpctl_line(
    const char *line,
    OSDVolumeState *out_state,
    OSDVolumeParseStatus *out_status
) {
    const char *cursor = NULL;
    const char *prefix_end = NULL;
    const char *end_ptr = NULL;
    double parsed_value = 0.0;
    double parsed_fraction = 0.0;
    bool parsed_as_percent = false;
    bool muted = false;

    if (line == NULL || out_state == NULL || out_status == NULL) {
        set_status(out_status, OSD_VOLUME_PARSE_ERR_INVALID_ARG);
        return false;
    }

    // Reset status before parsing so callers never read stale values
    set_status(out_status, OSD_VOLUME_PARSE_ERR_NONE);

    cursor = skip_whitespace(line);
    if (cursor == NULL) {
        set_status(out_status, OSD_VOLUME_PARSE_ERR_UNEXPECTED_FORMAT);
        return false;
    }

    // Support both common wpctl line forms used across versions
    if (strncmp(cursor, "Volume:", 7) == 0) {
        prefix_end = cursor + 7;
    } else if (strncmp(cursor, "Volume for ", 11) == 0) {
        prefix_end = strchr(cursor, ':');
        if (prefix_end != NULL) {
            prefix_end++;
        }
    }

    if (prefix_end == NULL) {
        set_status(out_status, OSD_VOLUME_PARSE_ERR_UNEXPECTED_FORMAT);
        return false;
    }

    cursor = skip_whitespace(prefix_end);
    if (cursor == NULL || *cursor == '\0') {
        set_status(out_status, OSD_VOLUME_PARSE_ERR_UNEXPECTED_FORMAT);
        return false;
    }

    // Numeric parse is locale-independent and only accepts dot decimals
    if (!parse_ascii_decimal_token(cursor, &end_ptr, &parsed_value)) {
        set_status(out_status, OSD_VOLUME_PARSE_ERR_INVALID_VALUE);
        return false;
    }

    // Optional percent marker switches from fraction to percentage input
    cursor = skip_whitespace(end_ptr);
    if (cursor != NULL && *cursor == '%') {
        parsed_as_percent = true;
        cursor = skip_whitespace(cursor + 1);
    }

    // Optional muted marker is consumed when present
    if (cursor != NULL && strncmp(cursor, "[MUTED]", 7) == 0) {
        muted = true;
        cursor = skip_whitespace(cursor + 7);
    }

    // Trailing bytes indicate unsupported format drift
    if (cursor != NULL && *cursor != '\0') {
        set_status(out_status, OSD_VOLUME_PARSE_ERR_UNEXPECTED_TAIL);
        return false;
    }

    // Normalize to UI scale where 1.00 maps to 100 and clamp hard upper bound
    parsed_fraction = parsed_as_percent ? (parsed_value / 100.0) : parsed_value;
    out_state->volume_percent = clamp_int((int)lround(parsed_fraction * 100.0), 0, 200);
    out_state->muted = muted;
    return true;
}
