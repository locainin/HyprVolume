#ifndef HYPRVOLUME_STYLE_ICON_H
#define HYPRVOLUME_STYLE_ICON_H

#include "args/args.h"

// Maps normalized volume state to symbolic icon name
const char *osd_style_icon_name_for_state_internal(const OSDVolumeState *state);

#endif
