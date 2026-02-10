#ifndef HYPRVOLUME_CONFIG_APPLY_H
#define HYPRVOLUME_CONFIG_APPLY_H

#include "args/args.h"

#include <stdbool.h>
#include <stdio.h>

bool osd_config_apply_numeric_values(const char *json_text, OSDArgs *args, FILE *err_stream);
bool osd_config_apply_monitor_index(const char *json_text, OSDArgs *args, FILE *err_stream);
bool osd_config_apply_bool_values(const char *json_text, OSDArgs *args, FILE *err_stream);
bool osd_config_apply_visual_values(const char *json_text, OSDArgs *args, FILE *err_stream);

#endif
