#ifndef OSD_ARGS_PARSE_VALUE_H
#define OSD_ARGS_PARSE_VALUE_H

#include <stdbool.h>
#include <stdio.h>

// Parses strict base-10 signed integer text and rejects trailing bytes
bool osd_args_parse_long_int(const char *text, long *out_value);

// Parses and validates bounded signed integer option values
bool osd_args_parse_ranged_int(
    const char *option_name,
    const char *value_text,
    int min_value,
    int max_value,
    int *out_value,
    FILE *err_stream
);

// Parses and validates bounded unsigned integer option values
bool osd_args_parse_ranged_uint(
    const char *option_name,
    const char *value_text,
    unsigned int min_value,
    unsigned int max_value,
    unsigned int *out_value,
    FILE *err_stream
);

#endif
