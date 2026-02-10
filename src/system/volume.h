#ifndef HYPRVOLUME_SYSTEM_VOLUME_H
#define HYPRVOLUME_SYSTEM_VOLUME_H

#include "args/args.h"

#include <stdbool.h>
#include <stdio.h>

// Queries wpctl and fills out_state with normalized values
// Returns false on resolve, process, or parse failure
// Writes one canonical error line to err_stream when a failure occurs
// Optional override path only works when HYPRVOLUME_ALLOW_WPCTL_PATH_OVERRIDE is "1"
bool osd_system_volume_query(OSDVolumeState *out_state, FILE *err_stream);

#endif
