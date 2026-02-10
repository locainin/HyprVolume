#ifndef HYPRVOLUME_CONFIG_SCHEMA_H
#define HYPRVOLUME_CONFIG_SCHEMA_H

#include <stdbool.h>
#include <stdio.h>

bool osd_config_schema_validate_top_level_keys(const char *json_text, FILE *err_stream);

#endif
