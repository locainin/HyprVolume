#ifndef HYPRVOLUME_SYSTEM_VOLUME_H
#define HYPRVOLUME_SYSTEM_VOLUME_H

#include "args/args.h"

#include <stdbool.h>
#include <stdio.h>

bool osd_system_volume_query(OSDVolumeState *out_state, FILE *err_stream);

#endif
