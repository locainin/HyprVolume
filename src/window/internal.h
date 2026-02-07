#ifndef WINDOW_INTERNAL_H
#define WINDOW_INTERNAL_H

#include "args/args.h"

#include <gtk/gtk.h>

typedef struct {
    OSDArgs args;
    OSDVolumeState current_volume;
    GtkWidget *window;
    GtkWidget *icon_overlay;
    GtkWidget *icon_image;
    GtkWidget *muted_slash;
    GtkWidget *progress_bar;
    GtkWidget *percent_label;
    GtkCssProvider *css_provider;
    GtkCssProvider *custom_css_provider;
    guint timeout_source_id;
    guint watch_source_id;
    unsigned int watch_idle_poll_ms;
    bool app_held;
    bool popup_visible;
    bool has_previous_watch_sample;
    bool watch_query_failure_logged;
    int exit_code;
} WindowState;

void window_apply_placement(WindowState *state);
void window_build_widgets(WindowState *state);
bool window_init_css(WindowState *state);

#endif
