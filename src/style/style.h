#ifndef HYPRVOLUME_STYLE_H
#define HYPRVOLUME_STYLE_H

#include "args/args.h"

#include <gtk/gtk.h>
#include <stdio.h>

GtkCssProvider *osd_style_build_provider(const OSDTheme *theme);
bool osd_style_build_custom_provider(const char *path, GtkCssProvider **out_provider, FILE *err_stream);
const char *osd_style_icon_name_for_state(const OSDVolumeState *state);

#endif
