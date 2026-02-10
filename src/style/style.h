#ifndef HYPRVOLUME_STYLE_H
#define HYPRVOLUME_STYLE_H

#include "args/args.h"

#include <gtk/gtk.h>
#include <stdio.h>

// Builds the base CSS provider from theme values
// Caller must release the returned provider with g_object_unref
GtkCssProvider *osd_style_build_provider(const OSDTheme *theme);

// Loads custom CSS from path into out_provider
// out_provider must point to NULL on entry
// Caller must release *out_provider with g_object_unref on success
bool osd_style_build_custom_provider(const char *path, GtkCssProvider **out_provider, FILE *err_stream);

// Maps volume state to a symbolic icon name
// Returned string has static lifetime
const char *osd_style_icon_name_for_state(const OSDVolumeState *state);

#endif
