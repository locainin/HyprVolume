#ifndef HYPRVOLUME_CONFIG_H
#define HYPRVOLUME_CONFIG_H

#include "args/args.h"

#include <stdbool.h>
#include <stdio.h>

bool osd_config_apply_file(const char *path, OSDArgs *args, FILE *err_stream);

#endif
