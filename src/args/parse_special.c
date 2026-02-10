#include "parse_internal.h"

#include "common/safeio.h"
#include "parse_option.h"
#include "parse_text.h"
#include "parse_value.h"

#include <string.h>

// Shared bounded path parser for --config and --css-file
// Keeps value extraction policy and copy limits in one place
static OSDParseDispatch parse_bounded_path_option(
    int argc,
    char **argv,
    int *index,
    const char *arg,
    const char *option_name,
    char *target,
    size_t target_size,
    bool reject_empty,
    const char *too_long_message,
    const char *empty_message,
    bool *out_is_set,
    FILE *err_stream
) {
    const char *inline_value = NULL;
    const char *value_text = NULL;

    // Fast skip when current token does not target this option
    if (!osd_args_match_option_with_value(arg, option_name, &inline_value)) {
        return OSD_PARSE_NOT_MATCHED;
    }

    // Text strict policy blocks accidental capture of other flags
    if (!osd_args_extract_option_value(
            argc,
            argv,
            index,
            option_name,
            inline_value,
            OSD_VALUE_POLICY_TEXT_STRICT,
            &value_text,
            err_stream
        )) {
        return OSD_PARSE_ERROR;
    }

    // Optional empty guard is used by --css-file only
    if (reject_empty && value_text[0] == '\0') {
        (void)osd_io_write_line(err_stream, empty_message);
        return OSD_PARSE_ERROR;
    }

    // Bounded copy prevents path overflow into fixed runtime buffers
    if (!osd_args_copy_text_bounded(target, target_size, value_text)) {
        (void)osd_io_write_line(err_stream, too_long_message);
        return OSD_PARSE_ERROR;
    }

    *out_is_set = true;
    return OSD_PARSE_MATCHED;
}

OSDParseDispatch osd_args_parse_config_option(
    int argc,
    char **argv,
    int *index,
    const char *arg,
    OSDArgs *out,
    FILE *err_stream
) {
    // Empty path is allowed here so caller can still stat and report real file error
    return parse_bounded_path_option(
        argc,
        argv,
        index,
        arg,
        "--config",
        out->config_path,
        sizeof(out->config_path),
        false,
        "Value for --config is too long",
        "",
        &out->config_path_set,
        err_stream
    );
}

OSDParseDispatch osd_args_parse_css_file_option(
    int argc,
    char **argv,
    int *index,
    const char *arg,
    OSDArgs *out,
    FILE *err_stream
) {
    // CSS path requires a non-empty value for clearer user feedback
    return parse_bounded_path_option(
        argc,
        argv,
        index,
        arg,
        "--css-file",
        out->css_path,
        sizeof(out->css_path),
        true,
        "Value for --css-file is too long",
        "Value for --css-file cannot be empty",
        &out->css_path_set,
        err_stream
    );
}

OSDParseDispatch osd_args_parse_toggle_flags(const char *arg, OSDArgs *out) {
    // Watch mode always implies system volume reads
    if (strcmp(arg, "--watch") == 0) {
        out->watch_mode = true;
        out->use_system_volume = true;
        return OSD_PARSE_MATCHED;
    }

    // --one-shot alias maps to disabled watch polling
    if (strcmp(arg, "--no-watch") == 0 || strcmp(arg, "--one-shot") == 0) {
        out->watch_mode = false;
        return OSD_PARSE_MATCHED;
    }

    if (strcmp(arg, "--from-system") == 0) {
        out->use_system_volume = true;
        return OSD_PARSE_MATCHED;
    }

    if (strcmp(arg, "--no-system") == 0) {
        out->use_system_volume = false;
        return OSD_PARSE_MATCHED;
    }

    if (strcmp(arg, "--muted") == 0) {
        out->volume.muted = true;
        return OSD_PARSE_MATCHED;
    }

    if (strcmp(arg, "--unmuted") == 0) {
        out->volume.muted = false;
        return OSD_PARSE_MATCHED;
    }

    if (strcmp(arg, "--css-replace") == 0) {
        out->css_replace = true;
        return OSD_PARSE_MATCHED;
    }

    // Append mode preserves built-in CSS then layers custom rules
    if (strcmp(arg, "--css-append") == 0) {
        out->css_replace = false;
        return OSD_PARSE_MATCHED;
    }

    if (strcmp(arg, "--vertical") == 0) {
        out->theme.vertical_layout = true;
        return OSD_PARSE_MATCHED;
    }

    if (strcmp(arg, "--horizontal") == 0) {
        out->theme.vertical_layout = false;
        return OSD_PARSE_MATCHED;
    }

    return OSD_PARSE_NOT_MATCHED;
}

OSDParseDispatch osd_args_parse_value_option(
    int argc,
    char **argv,
    int *index,
    const char *arg,
    OSDArgs *out,
    FILE *err_stream
) {
    const char *inline_value = NULL;
    const char *value_text = NULL;
    int parsed_value = 0;

    if (!osd_args_match_option_with_value(arg, "--value", &inline_value)) {
        return OSD_PARSE_NOT_MATCHED;
    }

    // Signed policy allows explicit negatives to reach numeric validator
    if (!osd_args_extract_option_value(
            argc,
            argv,
            index,
            "--value",
            inline_value,
            OSD_VALUE_POLICY_NUMERIC_SIGNED,
            &value_text,
            err_stream
        )) {
        return OSD_PARSE_ERROR;
    }

    // Runtime policy clamps manual values to display-safe [0, 200]
    if (!osd_args_parse_ranged_int("--value", value_text, 0, 200, &parsed_value, err_stream)) {
        return OSD_PARSE_ERROR;
    }

    // Explicit manual value disables system-source query path
    out->volume.volume_percent = parsed_value;
    out->use_system_volume = false;
    return OSD_PARSE_MATCHED;
}
