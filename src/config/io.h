#ifndef HYPRVOLUME_CONFIG_IO_H
#define HYPRVOLUME_CONFIG_IO_H

#include <stdbool.h>
#include <stdio.h>

bool osd_config_read_file_text(const char *path, char **out_json_text, FILE *err_stream);

#endif
