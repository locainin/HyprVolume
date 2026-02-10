#include "parse_internal.h"

#include "common/safeio.h"
#include "parse_option.h"
#include "parse_text.h"
#include "parse_value.h"

#include <string.h>

// Accessor callbacks keep dispatch loop generic while wrappers stay type-safe
typedef const char *(*OSDOptionNameAtFn)(const void *options, size_t option_index);
typedef OSDOptionValuePolicy (*OSDOptionPolicyAtFn)(const void *options, size_t option_index);
typedef bool (*OSDOptionApplyAtFn)(const void *options, size_t option_index, const char *value_text, FILE *err_stream);

// Shared dispatch core
// Matches option name, extracts value, applies typed parser, and reports one status
static OSDParseDispatch dispatch_option_table(
    int argc,
    char **argv,
    int *index,
    const char *arg,
    const void *options,
    size_t option_count,
    OSDOptionNameAtFn name_at,
    OSDOptionPolicyAtFn policy_at,
    OSDOptionApplyAtFn apply_at,
    FILE *err_stream
) {
    size_t option_index = 0U;

    if (arg == NULL || options == NULL || name_at == NULL || policy_at == NULL || apply_at == NULL) {
        return OSD_PARSE_NOT_MATCHED;
    }

    for (option_index = 0U; option_index < option_count; option_index++) {
        const char *inline_value = NULL;
        const char *value_text = NULL;
        const char *option_name = name_at(options, option_index);

        if (!osd_args_match_option_with_value(arg, option_name, &inline_value)) {
            continue;
        }

        // Value extraction controls argv index advances in one place
        if (!osd_args_extract_option_value(
                argc,
                argv,
                index,
                option_name,
                inline_value,
                policy_at(options, option_index),
                &value_text,
                err_stream
            )) {
            return OSD_PARSE_ERROR;
        }

        // Typed apply step validates ranges or buffer constraints per option kind
        if (!apply_at(options, option_index, value_text, err_stream)) {
            return OSD_PARSE_ERROR;
        }

        // First match wins so duplicate long options cannot silently conflict
        return OSD_PARSE_MATCHED;
    }

    return OSD_PARSE_NOT_MATCHED;
}

// UInt wrapper accessors
static const char *uint_option_name_at(const void *options, size_t option_index) {
    const OSDUIntOption *typed_options = (const OSDUIntOption *)options;

    return typed_options[option_index].name;
}

// UInt options use unsigned numeric extraction policy
static OSDOptionValuePolicy uint_option_policy_at(const void *options, size_t option_index) {
    (void)options;
    (void)option_index;
    return OSD_VALUE_POLICY_NUMERIC_UNSIGNED;
}

// UInt apply callback runs bounded unsigned numeric parser
static bool uint_option_apply_at(const void *options, size_t option_index, const char *value_text, FILE *err_stream) {
    const OSDUIntOption *typed_options = (const OSDUIntOption *)options;

    return osd_args_parse_ranged_uint(
        typed_options[option_index].name,
        value_text,
        typed_options[option_index].min_value,
        typed_options[option_index].max_value,
        typed_options[option_index].target,
        err_stream
    );
}

// Int wrapper accessors
static const char *int_option_name_at(const void *options, size_t option_index) {
    const OSDIntOption *typed_options = (const OSDIntOption *)options;

    return typed_options[option_index].name;
}

// Int options allow signed numeric tokens like -1
static OSDOptionValuePolicy int_option_policy_at(const void *options, size_t option_index) {
    (void)options;
    (void)option_index;
    return OSD_VALUE_POLICY_NUMERIC_SIGNED;
}

// Int apply callback runs bounded signed numeric parser
static bool int_option_apply_at(const void *options, size_t option_index, const char *value_text, FILE *err_stream) {
    const OSDIntOption *typed_options = (const OSDIntOption *)options;

    return osd_args_parse_ranged_int(
        typed_options[option_index].name,
        value_text,
        typed_options[option_index].min_value,
        typed_options[option_index].max_value,
        typed_options[option_index].target,
        err_stream
    );
}

// Shared bounded text setter for CSS color values
static bool set_color_value(
    char *target,
    size_t target_size,
    const char *option_name,
    const char *value_text,
    FILE *err_stream
) {
    const char *safe_value = (value_text == NULL) ? "(null)" : value_text;

    // Reject null, empty, and overflow cases before mutating destination
    if (value_text == NULL || value_text[0] == '\0' || !osd_args_copy_text_bounded(target, target_size, value_text)) {
        (void)osd_io_write_text(err_stream, "Invalid value for ");
        (void)osd_io_write_text(err_stream, option_name);
        (void)osd_io_write_text(err_stream, ": '");
        (void)osd_io_write_text(err_stream, safe_value);
        (void)osd_io_write_line(err_stream, "'");
        return false;
    }

    return true;
}

// Color wrapper accessors
static const char *color_option_name_at(const void *options, size_t option_index) {
    const OSDColorOption *typed_options = (const OSDColorOption *)options;

    return typed_options[option_index].name;
}

// Color strings follow strict text policy to avoid accidental option capture
static OSDOptionValuePolicy color_option_policy_at(const void *options, size_t option_index) {
    (void)options;
    (void)option_index;
    return OSD_VALUE_POLICY_TEXT_STRICT;
}

// Color apply callback performs fixed-buffer copy validation
static bool color_option_apply_at(const void *options, size_t option_index, const char *value_text, FILE *err_stream) {
    const OSDColorOption *typed_options = (const OSDColorOption *)options;

    return set_color_value(
        typed_options[option_index].target,
        typed_options[option_index].target_size,
        typed_options[option_index].name,
        value_text,
        err_stream
    );
}

OSDParseDispatch osd_args_parse_uint_options(
    int argc,
    char **argv,
    int *index,
    const char *arg,
    const OSDUIntOption *options,
    size_t option_count,
    FILE *err_stream
) {
    // Thin typed wrapper around shared dispatch core
    return dispatch_option_table(
        argc,
        argv,
        index,
        arg,
        options,
        option_count,
        uint_option_name_at,
        uint_option_policy_at,
        uint_option_apply_at,
        err_stream
    );
}

OSDParseDispatch osd_args_parse_int_options(
    int argc,
    char **argv,
    int *index,
    const char *arg,
    const OSDIntOption *options,
    size_t option_count,
    FILE *err_stream
) {
    // Thin typed wrapper around shared dispatch core
    return dispatch_option_table(
        argc,
        argv,
        index,
        arg,
        options,
        option_count,
        int_option_name_at,
        int_option_policy_at,
        int_option_apply_at,
        err_stream
    );
}

OSDParseDispatch osd_args_parse_color_options(
    int argc,
    char **argv,
    int *index,
    const char *arg,
    const OSDColorOption *options,
    size_t option_count,
    FILE *err_stream
) {
    // Thin typed wrapper around shared dispatch core
    return dispatch_option_table(
        argc,
        argv,
        index,
        arg,
        options,
        option_count,
        color_option_name_at,
        color_option_policy_at,
        color_option_apply_at,
        err_stream
    );
}
