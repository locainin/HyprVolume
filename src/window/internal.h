#ifndef WINDOW_INTERNAL_H
#define WINDOW_INTERNAL_H

#include "args/args.h"

#include <gtk/gtk.h>

// Shared runtime state for the GTK window flow
typedef struct {
    // Final parsed arguments copied at startup
    OSDArgs args;
    // Latest sampled system or manual volume state
    OSDVolumeState current_volume;
    // Root GTK window
    GtkWidget *window;
    // Overlay container for icon and mute slash
    GtkWidget *icon_overlay;
    // Icon widget for current volume bucket
    GtkWidget *icon_image;
    // Slash marker shown while muted
    GtkWidget *muted_slash;
    // Progress bar for normalized volume fraction
    GtkWidget *progress_bar;
    // Label widget showing percent or MUTED
    GtkWidget *percent_label;
    // Base generated CSS provider
    GtkCssProvider *css_provider;
    // Optional user supplied CSS provider
    GtkCssProvider *custom_css_provider;
    // Auto hide timeout source id
    guint timeout_source_id;
    // Watch poll timeout source id
    guint watch_source_id;
    // Slower poll interval used while popup is hidden
    unsigned int watch_idle_poll_ms;
    // Indicates app hold was acquired for watch mode
    bool app_held;
    // Tracks current popup visibility
    bool popup_visible;
    // Tracks whether watch mode has a baseline sample
    bool has_previous_watch_sample;
    // Prevents repeated watch failure spam
    bool watch_query_failure_logged;
    // Non zero value forces process error exit
    int exit_code;
} WindowState;

// Applies compositor placement and layer shell geometry rules
void window_apply_placement(WindowState *state);
// Builds static widget tree for icon bar and label
void window_build_widgets(WindowState *state);
// Installs base and optional custom CSS providers
bool window_init_css(WindowState *state);
// Records fatal error and requests GTK shutdown
void window_set_error(WindowState *state, const char *message);
// Renders icon bar and label from current volume state
void window_update_widgets(WindowState *state);
// Activates watch mode polling and popup behavior
bool window_activate_watch_mode(WindowState *state, GtkApplication *app);
// Activates one shot popup behavior
bool window_activate_single_popup(WindowState *state);

#endif
