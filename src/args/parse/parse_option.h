#ifndef OSD_ARGS_PARSE_OPTION_H
#define OSD_ARGS_PARSE_OPTION_H

#include <stdbool.h>
#include <stdio.h>

typedef enum {
    OSD_VALUE_POLICY_TEXT_STRICT = 0,
    OSD_VALUE_POLICY_NUMERIC_SIGNED = 1,
    OSD_VALUE_POLICY_NUMERIC_UNSIGNED = 2
} OSDOptionValuePolicy;

// Supports --name value and --name=value forms
bool osd_args_match_option_with_value(const char *arg, const char *name, const char **inline_value);

// Resolves inline or next-token value with policy checks for option-like tokens
// Supports explicit value escape as: --name -- value
bool osd_args_extract_option_value(
    int argc,
    char **argv,
    int *index,
    const char *option_name,
    const char *inline_value,
    OSDOptionValuePolicy policy,
    const char **out_value,
    FILE *err_stream
);

#endif
