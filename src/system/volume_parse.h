#ifndef HYPRVOLUME_SYSTEM_VOLUME_PARSE_H
#define HYPRVOLUME_SYSTEM_VOLUME_PARSE_H

#include "args/args.h"

#include <stdbool.h>

typedef enum {
    // Parsed line is valid and state can be consumed
    OSD_VOLUME_PARSE_ERR_NONE = 0,
    // Caller passed null or unusable buffers
    OSD_VOLUME_PARSE_ERR_INVALID_ARG,
    // Prefix did not match known wpctl output forms
    OSD_VOLUME_PARSE_ERR_UNEXPECTED_FORMAT,
    // Numeric token could not be parsed as a valid non-negative value
    OSD_VOLUME_PARSE_ERR_INVALID_VALUE,
    // Extra tokens remained after the supported output grammar
    OSD_VOLUME_PARSE_ERR_UNEXPECTED_TAIL
} OSDVolumeParseError;

typedef struct {
    // Last parse error classification
    OSDVolumeParseError error;
} OSDVolumeParseStatus;

// Parses one wpctl output line and writes normalized state
// Leading and trailing ASCII whitespace is accepted
// Numeric token uses ASCII digits with optional dot-decimal fraction
// Locale comma decimal forms are rejected by design
// Success initializes out_state and clamps percent to [0, 200]
bool osd_volume_parse_wpctl_line(
    const char *line,
    OSDVolumeState *out_state,
    OSDVolumeParseStatus *out_status
);

#endif
