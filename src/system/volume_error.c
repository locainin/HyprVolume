#include "system/volume_error_internal.h"

#include <ctype.h>

const char *osd_volume_error_path_source_label(OSDVolumePathSource source) {
    switch (source) {
    case OSD_VOLUME_PATH_SOURCE_OVERRIDE:
        return "override";
    case OSD_VOLUME_PATH_SOURCE_TRUSTED_CACHE:
        return "trusted-cache";
    case OSD_VOLUME_PATH_SOURCE_TRUSTED_DEFAULT:
        return "trusted-default";
    case OSD_VOLUME_PATH_SOURCE_UNKNOWN:
    default:
        return "unknown";
    }
}

const char *osd_volume_error_safe_detail_text(const char *detail) {
    return detail == NULL ? "(none)" : detail;
}

void osd_volume_error_sanitize_line_excerpt(const char *line, char *out_preview, size_t out_preview_size) {
    size_t input_index = 0U;
    size_t output_index = 0U;

    if (line == NULL || out_preview == NULL || out_preview_size == 0U) {
        return;
    }

    while (line[input_index] != '\0' && output_index < (out_preview_size - 1U)) {
        unsigned char value = (unsigned char)line[input_index];

        // Preview is single-line to avoid multiline log injection
        if (value == '\n' || value == '\r') {
            break;
        }

        // Preserve printable bytes and replace control bytes with safe markers
        if (isprint(value) != 0) {
            out_preview[output_index] = (char)value;
        } else if (isspace(value) != 0) {
            out_preview[output_index] = ' ';
        } else {
            out_preview[output_index] = '?';
        }

        input_index++;
        output_index++;
    }

    // Preview is always NUL-terminated for safe appends later
    out_preview[output_index] = '\0';
}

void osd_volume_error_line_init(OSDVolumeErrorLine *line) {
    if (line == NULL) {
        return;
    }

    line->used = 0U;
    line->text[0] = '\0';
}

void osd_volume_error_line_append_char(OSDVolumeErrorLine *line, char value) {
    if (line == NULL || line->used >= (OSD_VOLUME_ERROR_TEXT_MAX - 1U)) {
        // Silent truncation is intentional to avoid partial-overflow behavior
        return;
    }

    line->text[line->used] = value;
    line->used++;
    line->text[line->used] = '\0';
}

void osd_volume_error_line_append_text(OSDVolumeErrorLine *line, const char *text) {
    size_t index = 0U;

    if (line == NULL || text == NULL) {
        return;
    }

    while (text[index] != '\0' && line->used < (OSD_VOLUME_ERROR_TEXT_MAX - 1U)) {
        // Delegates boundary checks to the char append helper
        osd_volume_error_line_append_char(line, text[index]);
        index++;
    }
}

void osd_volume_error_line_append_int(OSDVolumeErrorLine *line, int value) {
    char reversed[32];
    size_t digit_count = 0U;
    unsigned long long magnitude = 0ULL;
    bool negative = false;

    if (line == NULL) {
        return;
    }

    if (value < 0) {
        long long promoted = (long long)value;

        // Promote first so INT_MIN negation stays defined
        negative = true;
        magnitude = (unsigned long long)(-promoted);
    } else {
        magnitude = (unsigned long long)value;
    }

    // Extract digits in reverse then append forward for final representation
    do {
        unsigned int digit = (unsigned int)(magnitude % 10ULL);

        reversed[digit_count] = (char)('0' + digit);
        digit_count++;
        magnitude /= 10ULL;
    } while (magnitude > 0ULL && digit_count < sizeof(reversed));

    if (negative) {
        // Sign is written before replaying reversed digits
        osd_volume_error_line_append_char(line, '-');
    }

    while (digit_count > 0U) {
        digit_count--;
        osd_volume_error_line_append_char(line, reversed[digit_count]);
    }
}

void osd_volume_error_line_emit(FILE *err_stream, const OSDVolumeErrorLine *line) {
    if (err_stream == NULL || line == NULL || line->used == 0U) {
        return;
    }

    // Single write keeps each message contiguous in common stderr usage
    (void)fwrite(line->text, 1U, line->used, err_stream);
}

void osd_volume_error_line_append_query_prefix(
    OSDVolumeErrorLine *line,
    const char *phase,
    OSDVolumePathSource source,
    const char *wpctl_path
) {
    // Shared prefix keeps all query errors aligned and grep-friendly
    osd_volume_error_line_append_text(line, "volume query failed: phase=");
    osd_volume_error_line_append_text(line, phase);
    osd_volume_error_line_append_text(line, " source=");
    osd_volume_error_line_append_text(line, osd_volume_error_path_source_label(source));
    osd_volume_error_line_append_text(line, " path=");
    osd_volume_error_line_append_text(line, wpctl_path);
    osd_volume_error_line_append_text(line, " reason=");
}
