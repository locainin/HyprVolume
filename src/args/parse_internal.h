#ifndef OSD_ARGS_PARSE_INTERNAL_H
#define OSD_ARGS_PARSE_INTERNAL_H

#include "args/args.h"

// Dispatch result used by each parser stage in the main option loop
typedef enum {
    // Current parser does not handle this token
    OSD_PARSE_NOT_MATCHED = 0,
    // Current parser consumed this token and updated output state
    OSD_PARSE_MATCHED = 1,
    // Current parser recognized token but value or constraints were invalid
    OSD_PARSE_ERROR = 2
} OSDParseDispatch;

// Unsigned numeric option descriptor used by generic dispatch loop
typedef struct {
    const char *name;
    unsigned int min_value;
    unsigned int max_value;
    unsigned int *target;
} OSDUIntOption;

// Signed numeric option descriptor used by generic dispatch loop
typedef struct {
    const char *name;
    int min_value;
    int max_value;
    int *target;
} OSDIntOption;

// Text option descriptor for color-like CSS values
typedef struct {
    const char *name;
    char *target;
    size_t target_size;
} OSDColorOption;

// Parses uint option tables with shared argv matching and extraction rules
OSDParseDispatch osd_args_parse_uint_options(
    int argc,
    char **argv,
    int *index,
    const char *arg,
    const OSDUIntOption *options,
    size_t option_count,
    FILE *err_stream
);

// Parses int option tables with shared argv matching and extraction rules
OSDParseDispatch osd_args_parse_int_options(
    int argc,
    char **argv,
    int *index,
    const char *arg,
    const OSDIntOption *options,
    size_t option_count,
    FILE *err_stream
);

// Parses text option tables and copies validated values into fixed buffers
OSDParseDispatch osd_args_parse_color_options(
    int argc,
    char **argv,
    int *index,
    const char *arg,
    const OSDColorOption *options,
    size_t option_count,
    FILE *err_stream
);

// Parses --config path and marks config_path_set on success
OSDParseDispatch osd_args_parse_config_option(
    int argc,
    char **argv,
    int *index,
    const char *arg,
    OSDArgs *out,
    FILE *err_stream
);

// Parses --css-file path and marks css_path_set on success
OSDParseDispatch osd_args_parse_css_file_option(
    int argc,
    char **argv,
    int *index,
    const char *arg,
    OSDArgs *out,
    FILE *err_stream
);

// Parses flag-style toggles that do not require value extraction
OSDParseDispatch osd_args_parse_toggle_flags(const char *arg, OSDArgs *out);

// Parses --value and switches runtime source to manual mode
OSDParseDispatch osd_args_parse_value_option(
    int argc,
    char **argv,
    int *index,
    const char *arg,
    OSDArgs *out,
    FILE *err_stream
);

#endif
