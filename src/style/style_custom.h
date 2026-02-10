#ifndef HYPRVOLUME_STYLE_CUSTOM_H
#define HYPRVOLUME_STYLE_CUSTOM_H

#include <gtk/gtk.h>
#include <stdio.h>

// Loads custom CSS provider from filesystem path
// path must reference a regular UTF-8 text file
// out_provider must be non-null and point to NULL on entry
// caller owns returned provider and must release with g_object_unref
// returns false and keeps out_provider as NULL on failure
bool osd_style_custom_build_provider(const char *path, GtkCssProvider **out_provider, FILE *err_stream);

#endif
