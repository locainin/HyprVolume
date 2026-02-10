#ifndef HYPRVOLUME_CONFIG_ERROR_H
#define HYPRVOLUME_CONFIG_ERROR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

void osd_config_write_error_text(FILE *err_stream, const char *text);
void osd_config_write_error_value_message(FILE *err_stream, const char *prefix, const char *value, const char *suffix);
bool osd_config_copy_cstr_bounded(char *destination, size_t destination_size, const char *source);

#endif
