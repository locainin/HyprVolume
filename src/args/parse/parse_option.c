#include "args/parse/parse_option.h"

#include "common/safeio.h"
#include "args/parse/parse_value.h"

#include <string.h>

// Treats any non-empty '-' prefixed token as option-like syntax
static bool token_looks_like_option(const char *token) {
    return token != NULL && token[0] == '-' && token[1] != '\0';
}

// Signed numeric policy allows values like -1 as real numbers
static bool token_is_signed_number(const char *token) {
    long parsed = 0L;

    return osd_args_parse_long_int(token, &parsed);
}

// Central policy gate that decides whether next token can be consumed as value
static bool policy_rejects_next_token(OSDOptionValuePolicy policy, const char *candidate_token) {
    if (!token_looks_like_option(candidate_token)) {
        return false;
    }

    switch (policy) {
    // Text options and unsigned numbers both reject option-looking tokens
    case OSD_VALUE_POLICY_TEXT_STRICT:
    case OSD_VALUE_POLICY_NUMERIC_UNSIGNED:
        return true;
    // Signed options allow -N but still reject true option names
    case OSD_VALUE_POLICY_NUMERIC_SIGNED:
        return !token_is_signed_number(candidate_token);
    default:
        return false;
    }
}

// Supports explicit value escape as: --name -- value
static bool token_is_end_of_options(const char *token) {
    return token != NULL && strcmp(token, "--") == 0;
}

bool osd_args_match_option_with_value(const char *arg, const char *name, const char **inline_value) {
    size_t name_len = 0U;

    if (arg == NULL || name == NULL || inline_value == NULL) {
        return false;
    }

    name_len = strlen(name);

    // --name form consumes value from following argv token
    if (strcmp(arg, name) == 0) {
        *inline_value = NULL;
        return true;
    }

    // --name=value form keeps value inline and avoids argv index shifts
    if (strncmp(arg, name, name_len) == 0 && arg[name_len] == '=') {
        *inline_value = arg + name_len + 1;
        return true;
    }

    return false;
}

bool osd_args_extract_option_value(
    int argc,
    char **argv,
    int *index,
    const char *option_name,
    const char *inline_value,
    OSDOptionValuePolicy policy,
    const char **out_value,
    FILE *err_stream
) {
    const char *candidate_value = NULL;

    if (argv == NULL || index == NULL || option_name == NULL || out_value == NULL || err_stream == NULL) {
        return false;
    }

    // Inline values are validated in place and do not consume extra argv slots
    if (inline_value != NULL) {
        if (*inline_value == '\0') {
            (void)osd_io_write_text(err_stream, "Empty value for ");
            (void)osd_io_write_line(err_stream, option_name);
            return false;
        }

        *out_value = inline_value;
        return true;
    }

    if ((*index + 1) >= argc) {
        (void)osd_io_write_text(err_stream, "Missing value after ");
        (void)osd_io_write_line(err_stream, option_name);
        return false;
    }

    candidate_value = argv[*index + 1];
    // Explicit end-of-options marker lets text values start with '-'
    if (token_is_end_of_options(candidate_value)) {
        if ((*index + 2) >= argc) {
            (void)osd_io_write_text(err_stream, "Missing value after ");
            (void)osd_io_write_line(err_stream, option_name);
            return false;
        }

        // Consumes both "--" and the actual value token
        *index += 2;
        *out_value = argv[*index];
        return true;
    }

    // Policy check blocks accidental option capture like --config --watch
    if (policy_rejects_next_token(policy, candidate_value)) {
        (void)osd_io_write_text(err_stream, "Missing value after ");
        (void)osd_io_write_line(err_stream, option_name);
        return false;
    }

    *index += 1;
    *out_value = candidate_value;
    return true;
}
