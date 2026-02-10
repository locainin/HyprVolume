#include "internal.h"

#include <gtk4-layer-shell.h>

// Resolves monitor selection and falls back to default monitor on errors
static GdkMonitor *window_get_target_monitor(int monitor_index) {
    GdkDisplay *display = NULL;
    GListModel *monitors = NULL;
    guint monitor_count = 0U;
    guint selected_index = 0U;

    display = gdk_display_get_default();
    if (display == NULL) {
        // No display means compositor session is unavailable
        return NULL;
    }

    monitors = gdk_display_get_monitors(display);
    monitor_count = g_list_model_get_n_items(monitors);
    if (monitor_count == 0U) {
        // Monitor list can be empty during transient display changes
        return NULL;
    }

    if (monitor_index >= 0) {
        // Negative index keeps default monitor index
        selected_index = (guint)monitor_index;
    }

    if (selected_index >= monitor_count) {
        g_warning(
            "Requested monitor index %d is out of range [0, %u); using default monitor",
            monitor_index,
            monitor_count
        );
        selected_index = 0U;
    }

    // Caller owns the reference and must call g_object_unref
    return GDK_MONITOR(g_list_model_get_item(monitors, selected_index));
}

// Converts 0..100 placement percentages into edge margins
static int window_percent_to_margin(int percent, int monitor_size, unsigned int widget_size) {
    int available = monitor_size - (int)widget_size;

    if (available < 0) {
        // Oversized widgets clamp to edge aligned placement
        available = 0;
    }

    // +50 keeps integer rounding stable for percent conversion
    return (available * percent + 50) / 100;
}

// Clears all anchors and margins before reapplying placement mode
static void window_reset_layer_edges(GtkWindow *window) {
    // Layer shell placement starts from a neutral anchor and margin state
    gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_TOP, FALSE);
    gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_BOTTOM, FALSE);
    gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, FALSE);
    gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_RIGHT, FALSE);
    gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_TOP, 0);
    gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_BOTTOM, 0);
    gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_LEFT, 0);
    gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_RIGHT, 0);
}

// Applies anchor driven placement used when x and y percent are not configured
static void window_apply_anchor_placement(const WindowState *state) {
    GtkWindow *window = GTK_WINDOW(state->window);

    // Switch keeps per anchor edge and margin mapping explicit
    switch (state->args.theme.anchor) {
        case OSD_ANCHOR_TOP_CENTER:
            gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
            gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_TOP, (int)state->args.theme.margin_y_px);
            break;
        case OSD_ANCHOR_TOP_LEFT:
            gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
            gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
            gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_TOP, (int)state->args.theme.margin_y_px);
            gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_LEFT, (int)state->args.theme.margin_x_px);
            break;
        case OSD_ANCHOR_TOP_RIGHT:
            gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
            gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
            gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_TOP, (int)state->args.theme.margin_y_px);
            gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_RIGHT, (int)state->args.theme.margin_x_px);
            break;
        case OSD_ANCHOR_BOTTOM_CENTER:
            gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
            gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_BOTTOM, (int)state->args.theme.margin_y_px);
            break;
        case OSD_ANCHOR_BOTTOM_LEFT:
            gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
            gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
            gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_BOTTOM, (int)state->args.theme.margin_y_px);
            gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_LEFT, (int)state->args.theme.margin_x_px);
            break;
        case OSD_ANCHOR_BOTTOM_RIGHT:
            gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
            gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
            gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_BOTTOM, (int)state->args.theme.margin_y_px);
            gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_RIGHT, (int)state->args.theme.margin_x_px);
            break;
        default:
            gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
            gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_TOP, (int)state->args.theme.margin_y_px);
            break;
    }
}

// Applies explicit percent placement relative to monitor geometry
static void window_apply_percent_placement(WindowState *state, GdkMonitor *monitor) {
    GdkRectangle geometry;
    int margin_left = 0;
    int margin_top = 0;

    gdk_monitor_get_geometry(monitor, &geometry);
    // Percent coordinates are interpreted relative to monitor work area
    margin_left = window_percent_to_margin(state->args.theme.x_percent, geometry.width, state->args.theme.width_px);
    margin_top = window_percent_to_margin(state->args.theme.y_percent, geometry.height, state->args.theme.height_px);

    gtk_layer_set_anchor(GTK_WINDOW(state->window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(state->window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_margin(GTK_WINDOW(state->window), GTK_LAYER_SHELL_EDGE_LEFT, margin_left);
    gtk_layer_set_margin(GTK_WINDOW(state->window), GTK_LAYER_SHELL_EDGE_TOP, margin_top);
}

// Configures the window as a layer shell overlay and selects placement mode
void window_apply_placement(WindowState *state) {
    GdkMonitor *target_monitor = NULL;

    if (!gtk_layer_is_supported()) {
        g_warning("layer-shell protocol unavailable; falling back to regular GTK popup window");
        return;
    }

    gtk_layer_init_for_window(GTK_WINDOW(state->window));
    // Namespace enables compositor side targeted rules for this overlay
    gtk_layer_set_namespace(GTK_WINDOW(state->window), "hyprvolume");
    gtk_layer_set_layer(GTK_WINDOW(state->window), GTK_LAYER_SHELL_LAYER_OVERLAY);
    window_reset_layer_edges(GTK_WINDOW(state->window));

    target_monitor = window_get_target_monitor(state->args.monitor_index);
    if (target_monitor != NULL) {
        // Monitor pinning applies only when a valid monitor object exists
        gtk_layer_set_monitor(GTK_WINDOW(state->window), target_monitor);
    }

    if (state->args.theme.x_percent >= 0 && state->args.theme.y_percent >= 0 && target_monitor != NULL) {
        // Percent placement has priority when both coordinates are configured
        window_apply_percent_placement(state, target_monitor);
    } else {
        // Anchor placement is fallback for unset percent coordinates
        window_apply_anchor_placement(state);
    }

    if (target_monitor != NULL) {
        // Drop monitor reference returned by window_get_target_monitor
        g_object_unref(target_monitor);
    }

    // Overlay should never reserve layout space in compositor stacking
    gtk_layer_set_exclusive_zone(GTK_WINDOW(state->window), -1);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(state->window), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
}
