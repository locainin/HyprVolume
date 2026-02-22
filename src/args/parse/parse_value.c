#include "args/parse/parse_value.h"

#include "common/safeio.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

// Shared invalid-number message to keep wording identical across parsers
static void write_invalid_numeric_value(FILE *err_stream, const char *option_name, const char *value_text) {
    const char *safe_value = (value_text == NULL) ? "(null)" : value_text;

    (void)osd_io_write_text(err_stream, "Invalid numeric value for ");
    (void)osd_io_write_text(err_stream, option_name);
    (void)osd_io_write_text(err_stream, ": '");
    (void)osd_io_write_text(err_stream, safe_value);
    (void)osd_io_write_line(err_stream, "'");
}

// Signed range message helper used by int parser
static void write_signed_range_error(
    FILE *err_stream,
    const char *option_name,
    int min_value,
    int max_value,
    const char *value_text
) {
    (void)osd_io_write_text(err_stream, "Value for ");
    (void)osd_io_write_text(err_stream, option_name);
    (void)osd_io_write_text(err_stream, " must be in range [");
    (void)osd_io_write_long_long(err_stream, (long long)min_value);
    (void)osd_io_write_text(err_stream, ", ");
    (void)osd_io_write_long_long(err_stream, (long long)max_value);
    (void)osd_io_write_text(err_stream, "], got '");
    (void)osd_io_write_text(err_stream, value_text);
    (void)osd_io_write_line(err_stream, "'");
}

// Unsigned range message helper used by uint parser
static void write_unsigned_range_error(
    FILE *err_stream,
    const char *option_name,
    unsigned int min_value,
    unsigned int max_value,
    const char *value_text
) {
    (void)osd_io_write_text(err_stream, "Value for ");
    (void)osd_io_write_text(err_stream, option_name);
    (void)osd_io_write_text(err_stream, " must be in range [");
    (void)osd_io_write_unsigned_long_long(err_stream, (unsigned long long)min_value);
    (void)osd_io_write_text(err_stream, ", ");
    (void)osd_io_write_unsigned_long_long(err_stream, (unsigned long long)max_value);
    (void)osd_io_write_text(err_stream, "], got '");
    (void)osd_io_write_text(err_stream, value_text);
    (void)osd_io_write_line(err_stream, "'");
}

bool osd_args_parse_long_int(const char *text, long *out_value) {
    char *end_ptr = NULL;
    long parsed = 0L;

    // Reject null and empty strings before calling strtol
    if (text == NULL || out_value == NULL || *text == '\0') {
        return false;
    }

    // strtol handles leading sign and overflow reporting through errno
    errno = 0;
    parsed = strtol(text, &end_ptr, 10);
    // Full token must parse as numeric with no trailing bytes
    if (errno != 0 || end_ptr == NULL || end_ptr == text || *end_ptr != '\0') {
        return false;
    }

    *out_value = parsed;
    return true;
}

bool osd_args_parse_ranged_int(
    const char *option_name,
    const char *value_text,
    int min_value,
    int max_value,
    int *out_value,
    FILE *err_stream
) {
    long parsed_value = 0L;

    // Stage 1 parses text into wide signed type
    if (!osd_args_parse_long_int(value_text, &parsed_value)) {
        write_invalid_numeric_value(err_stream, option_name, value_text);
        return false;
    }

    // Stage 2 enforces option-level limits
    if (parsed_value < (long)min_value || parsed_value > (long)max_value) {
        write_signed_range_error(err_stream, option_name, min_value, max_value, value_text);
        return false;
    }

    // Stage 3 guards narrowing cast into int
    if (parsed_value < (long)INT_MIN || parsed_value > (long)INT_MAX) {
        write_invalid_numeric_value(err_stream, option_name, value_text);
        return false;
    }

    *out_value = (int)parsed_value;
    return true;
}

bool osd_args_parse_ranged_uint(
    const char *option_name,
    const char *value_text,
    unsigned int min_value,
    unsigned int max_value,
    unsigned int *out_value,
    FILE *err_stream
) {
    char *end_ptr = NULL;
    unsigned long parsed_value = 0UL;

    // Empty and null values are invalid for numeric options
    if (value_text == NULL || out_value == NULL || *value_text == '\0') {
        write_invalid_numeric_value(err_stream, option_name, value_text);
        return false;
    }

    // Unsigned options reject negative sign before conversion
    if (value_text[0] == '-') {
        write_unsigned_range_error(err_stream, option_name, min_value, max_value, value_text);
        return false;
    }

    // strtoul converts base-10 token and reports overflow through errno
    errno = 0;
    parsed_value = strtoul(value_text, &end_ptr, 10);
    // Full token must parse without trailing bytes
    if (errno != 0 || end_ptr == NULL || end_ptr == value_text || *end_ptr != '\0') {
        write_invalid_numeric_value(err_stream, option_name, value_text);
        return false;
    }

    // Validate against target type width and option bounds before narrowing
    if (parsed_value > (unsigned long)UINT_MAX ||
        parsed_value < (unsigned long)min_value ||
        parsed_value > (unsigned long)max_value) {
        write_unsigned_range_error(err_stream, option_name, min_value, max_value, value_text);
        return false;
    }

    *out_value = (unsigned int)parsed_value;
    return true;
}
