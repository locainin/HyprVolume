#include "parse_internal.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* Parses a strict base-10 integer and rejects trailing text. */
static bool parse_long_int(const char *text, long *out_value) {
    char *end_ptr = NULL;
    long parsed = 0L;

    if (text == NULL || *text == '\0') {
        return false;
    }

    errno = 0;
    parsed = strtol(text, &end_ptr, 10);
    if (errno != 0 || end_ptr == NULL || *end_ptr != '\0') {
        return false;
    }

    *out_value = parsed;
    return true;
}

/* Supports --name value and --name=value forms. */
static bool match_option_with_value(const char *arg, const char *name, const char **inline_value) {
    const size_t name_len = strlen(name);

    if (strcmp(arg, name) == 0) {
        *inline_value = NULL;
        return true;
    }

    if (strncmp(arg, name, name_len) == 0 && arg[name_len] == '=') {
        *inline_value = arg + name_len + 1;
        return true;
    }

    return false;
}

/* Resolves the option value from inline syntax or from the next argv entry. */
static bool extract_option_value(
    int argc,
    char **argv,
    int *index,
    const char *option_name,
    const char *inline_value,
    const char **out_value,
    FILE *err_stream
) {
    if (inline_value != NULL) {
        if (*inline_value == '\0') {
            (void)fprintf(err_stream, "Empty value for %s\n", option_name);
            return false;
        }

        *out_value = inline_value;
        return true;
    }

    if ((*index + 1) >= argc) {
        (void)fprintf(err_stream, "Missing value after %s\n", option_name);
        return false;
    }

    *index += 1;
    *out_value = argv[*index];
    return true;
}

/* Applies common range checks used by numeric options. */
static bool parse_ranged_int(
    const char *option_name,
    const char *value_text,
    int min_value,
    int max_value,
    int *out_value,
    FILE *err_stream
) {
    long parsed_value = 0L;

    if (!parse_long_int(value_text, &parsed_value)) {
        (void)fprintf(err_stream, "Invalid numeric value for %s: '%s'\n", option_name, value_text);
        return false;
    }

    if (parsed_value < (long)min_value || parsed_value > (long)max_value) {
        (void)fprintf(
            err_stream,
            "Value for %s must be in range [%d, %d], got '%s'\n",
            option_name,
            min_value,
            max_value,
            value_text
        );
        return false;
    }

    *out_value = (int)parsed_value;
    return true;
}

/* Unsigned range parser reuses the signed validator for shared behavior. */
static bool parse_ranged_uint(
    const char *option_name,
    const char *value_text,
    unsigned int min_value,
    unsigned int max_value,
    unsigned int *out_value,
    FILE *err_stream
) {
    int parsed_value = 0;

    if (!parse_ranged_int(option_name, value_text, (int)min_value, (int)max_value, &parsed_value, err_stream)) {
        return false;
    }

    *out_value = (unsigned int)parsed_value;
    return true;
}

/* Copies CSS-style values with explicit maximum length validation. */
static bool set_color_value(
    char *target,
    size_t target_size,
    const char *option_name,
    const char *value_text,
    FILE *err_stream
) {
    const size_t value_len = strlen(value_text);

    if (value_len == 0U || value_len >= target_size) {
        (void)fprintf(err_stream, "Invalid value for %s: '%s'\n", option_name, value_text);
        return false;
    }

    (void)memcpy(target, value_text, value_len + 1U);
    return true;
}

/* Parses bounded unsigned options and writes directly into their targets. */
OSDParseDispatch osd_args_parse_uint_options(
    int argc,
    char **argv,
    int *index,
    const char *arg,
    const OSDUIntOption *options,
    size_t option_count,
    FILE *err_stream
) {
    size_t option_index = 0U;

    for (option_index = 0U; option_index < option_count; option_index++) {
        const char *inline_value = NULL;
        const char *value_text = NULL;

        if (!match_option_with_value(arg, options[option_index].name, &inline_value)) {
            continue;
        }

        if (!extract_option_value(argc, argv, index, options[option_index].name, inline_value, &value_text, err_stream)) {
            return OSD_PARSE_ERROR;
        }

        if (!parse_ranged_uint(
                options[option_index].name,
                value_text,
                options[option_index].min_value,
                options[option_index].max_value,
                options[option_index].target,
                err_stream
            )) {
            return OSD_PARSE_ERROR;
        }

        return OSD_PARSE_MATCHED;
    }

    return OSD_PARSE_NOT_MATCHED;
}

/* Parses bounded signed options and writes directly into their targets. */
OSDParseDispatch osd_args_parse_int_options(
    int argc,
    char **argv,
    int *index,
    const char *arg,
    const OSDIntOption *options,
    size_t option_count,
    FILE *err_stream
) {
    size_t option_index = 0U;

    for (option_index = 0U; option_index < option_count; option_index++) {
        const char *inline_value = NULL;
        const char *value_text = NULL;

        if (!match_option_with_value(arg, options[option_index].name, &inline_value)) {
            continue;
        }

        if (!extract_option_value(argc, argv, index, options[option_index].name, inline_value, &value_text, err_stream)) {
            return OSD_PARSE_ERROR;
        }

        if (!parse_ranged_int(
                options[option_index].name,
                value_text,
                options[option_index].min_value,
                options[option_index].max_value,
                options[option_index].target,
                err_stream
            )) {
            return OSD_PARSE_ERROR;
        }

        return OSD_PARSE_MATCHED;
    }

    return OSD_PARSE_NOT_MATCHED;
}

/* Parses CSS color/gradient options with size-limited copies. */
OSDParseDispatch osd_args_parse_color_options(
    int argc,
    char **argv,
    int *index,
    const char *arg,
    const OSDColorOption *options,
    size_t option_count,
    FILE *err_stream
) {
    size_t option_index = 0U;

    for (option_index = 0U; option_index < option_count; option_index++) {
        const char *inline_value = NULL;
        const char *value_text = NULL;

        if (!match_option_with_value(arg, options[option_index].name, &inline_value)) {
            continue;
        }

        if (!extract_option_value(argc, argv, index, options[option_index].name, inline_value, &value_text, err_stream)) {
            return OSD_PARSE_ERROR;
        }

        if (!set_color_value(
                options[option_index].target,
                options[option_index].target_size,
                options[option_index].name,
                value_text,
                err_stream
            )) {
            return OSD_PARSE_ERROR;
        }

        return OSD_PARSE_MATCHED;
    }

    return OSD_PARSE_NOT_MATCHED;
}

/* Reads the config path and marks it for the second parse pass. */
OSDParseDispatch osd_args_parse_config_option(
    int argc,
    char **argv,
    int *index,
    const char *arg,
    OSDArgs *out,
    FILE *err_stream
) {
    const char *inline_value = NULL;
    const char *value_text = NULL;

    if (!match_option_with_value(arg, "--config", &inline_value)) {
        return OSD_PARSE_NOT_MATCHED;
    }

    if (!extract_option_value(argc, argv, index, "--config", inline_value, &value_text, err_stream)) {
        return OSD_PARSE_ERROR;
    }

    if (strlen(value_text) >= sizeof(out->config_path)) {
        (void)fprintf(err_stream, "Value for --config is too long\n");
        return OSD_PARSE_ERROR;
    }

    (void)strcpy(out->config_path, value_text);
    out->config_path_set = true;
    return OSD_PARSE_MATCHED;
}

/* Reads custom CSS file path used to override or extend built-in styles. */
OSDParseDispatch osd_args_parse_css_file_option(
    int argc,
    char **argv,
    int *index,
    const char *arg,
    OSDArgs *out,
    FILE *err_stream
) {
    const char *inline_value = NULL;
    const char *value_text = NULL;

    if (!match_option_with_value(arg, "--css-file", &inline_value)) {
        return OSD_PARSE_NOT_MATCHED;
    }

    if (!extract_option_value(argc, argv, index, "--css-file", inline_value, &value_text, err_stream)) {
        return OSD_PARSE_ERROR;
    }

    if (strlen(value_text) >= sizeof(out->css_path)) {
        (void)fprintf(err_stream, "Value for --css-file is too long\n");
        return OSD_PARSE_ERROR;
    }

    if (*value_text == '\0') {
        (void)fprintf(err_stream, "Value for --css-file cannot be empty\n");
        return OSD_PARSE_ERROR;
    }

    (void)strcpy(out->css_path, value_text);
    out->css_path_set = true;
    return OSD_PARSE_MATCHED;
}

/* Handles the flag-only option set. */
OSDParseDispatch osd_args_parse_toggle_flags(const char *arg, OSDArgs *out) {
    if (strcmp(arg, "--watch") == 0) {
        out->watch_mode = true;
        out->use_system_volume = true;
        return OSD_PARSE_MATCHED;
    }

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

/* Handles the manual volume percentage option. */
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

    if (!match_option_with_value(arg, "--value", &inline_value)) {
        return OSD_PARSE_NOT_MATCHED;
    }

    if (!extract_option_value(argc, argv, index, "--value", inline_value, &value_text, err_stream)) {
        return OSD_PARSE_ERROR;
    }

    if (!parse_ranged_int("--value", value_text, 0, 200, &parsed_value, err_stream)) {
        return OSD_PARSE_ERROR;
    }

    out->volume.volume_percent = parsed_value;
    out->use_system_volume = false;
    return OSD_PARSE_MATCHED;
}
