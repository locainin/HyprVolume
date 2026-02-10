#ifndef HYPRVOLUME_STYLE_THEME_H
#define HYPRVOLUME_STYLE_THEME_H

#include "args/args.h"

#include <gtk/gtk.h>

// Builds provider from runtime theme values
GtkCssProvider *osd_style_theme_build_provider(const OSDTheme *theme);

#endif
