#ifndef OSD_ARGS_PARSE_INTERNAL_H
#define OSD_ARGS_PARSE_INTERNAL_H

#include "args/args.h"

typedef enum {
    OSD_PARSE_NOT_MATCHED = 0,
    OSD_PARSE_MATCHED = 1,
    OSD_PARSE_ERROR = 2
} OSDParseDispatch;

typedef struct {
    const char *name;
    unsigned int min_value;
    unsigned int max_value;
    unsigned int *target;
} OSDUIntOption;

typedef struct {
    const char *name;
    int min_value;
    int max_value;
    int *target;
} OSDIntOption;

typedef struct {
    const char *name;
    char *target;
    size_t target_size;
} OSDColorOption;

OSDParseDispatch osd_args_parse_uint_options(
    int argc,
    char **argv,
    int *index,
    const char *arg,
    const OSDUIntOption *options,
    size_t option_count,
    FILE *err_stream
);

OSDParseDispatch osd_args_parse_int_options(
    int argc,
    char **argv,
    int *index,
    const char *arg,
    const OSDIntOption *options,
    size_t option_count,
    FILE *err_stream
);

OSDParseDispatch osd_args_parse_color_options(
    int argc,
    char **argv,
    int *index,
    const char *arg,
    const OSDColorOption *options,
    size_t option_count,
    FILE *err_stream
);

OSDParseDispatch osd_args_parse_config_option(
    int argc,
    char **argv,
    int *index,
    const char *arg,
    OSDArgs *out,
    FILE *err_stream
);

OSDParseDispatch osd_args_parse_css_file_option(
    int argc,
    char **argv,
    int *index,
    const char *arg,
    OSDArgs *out,
    FILE *err_stream
);

OSDParseDispatch osd_args_parse_toggle_flags(const char *arg, OSDArgs *out);

OSDParseDispatch osd_args_parse_value_option(
    int argc,
    char **argv,
    int *index,
    const char *arg,
    OSDArgs *out,
    FILE *err_stream
);

#endif
