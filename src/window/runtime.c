#include "internal.h"

#include "system/volume.h"

// Forward declaration for one shot watch timer callback
static gboolean window_on_watch_poll(gpointer user_data);

// Compares sampled volume states to suppress redundant redraws
static bool volume_states_equal(const OSDVolumeState *a, const OSDVolumeState *b) {
    return a->volume_percent == b->volume_percent && a->muted == b->muted;
}

// Logs repeated watch failures once until recovery succeeds
static void window_log_watch_query_failure(WindowState *state, const char *message) {
    if (state == NULL || message == NULL) {
        return;
    }

    if (state->watch_query_failure_logged) {
        return;
    }

    g_printerr("%s\n", message);
    state->watch_query_failure_logged = true;
}

// Emits one recovery message after transient failures clear
static void window_log_watch_query_recovery(WindowState *state) {
    if (state == NULL || !state->watch_query_failure_logged) {
        return;
    }

    g_printerr("System volume query recovered; watch mode resumed\n");
    state->watch_query_failure_logged = false;
}

// Arms one shot watch polling using active or idle interval
static bool window_schedule_watch_poll(WindowState *state) {
    unsigned int next_poll_ms = 0U;

    if (state == NULL) {
        return false;
    }
    if (state->watch_source_id != 0U) {
        // Remove any stale timer before re arming
        g_source_remove(state->watch_source_id);
        state->watch_source_id = 0U;
    }

    // Visible popup uses active interval hidden popup uses idle interval
    next_poll_ms = state->popup_visible ? state->args.watch_poll_ms : state->watch_idle_poll_ms;
    state->watch_source_id = g_timeout_add(next_poll_ms, window_on_watch_poll, state);
    if (state->watch_source_id == 0U) {
        window_set_error(state, "Failed to start watch timer");
        return false;
    }

    return true;
}

// Handles auto hide timeout for single and watch modes
static gboolean window_on_timeout(gpointer user_data) {
    WindowState *state = user_data;

    // Timeout source is one shot clear stored id first
    state->timeout_source_id = 0U;
    if (state->args.watch_mode) {
        // Watch mode hides popup and keeps polling
        state->popup_visible = false;
        gtk_widget_set_visible(state->window, FALSE);
        return G_SOURCE_REMOVE;
    }

    if (g_application_get_default() != NULL) {
        g_application_quit(g_application_get_default());
    }

    return G_SOURCE_REMOVE;
}

// Restarts popup hide timer after show or refresh
static bool window_arm_timeout(WindowState *state) {
    if (state->timeout_source_id != 0U) {
        // Re arm avoids stale timeout callbacks
        g_source_remove(state->timeout_source_id);
        state->timeout_source_id = 0U;
    }

    state->timeout_source_id = g_timeout_add(state->args.timeout_ms, window_on_timeout, state);
    if (state->timeout_source_id == 0U) {
        window_set_error(state, "Failed to start popup timeout timer");
        return false;
    }

    return true;
}

// Shows popup and arms timeout handling
static bool window_show_popup(WindowState *state) {
    if (!state->popup_visible) {
        // Avoid repeated present calls while already visible
        gtk_widget_set_visible(state->window, TRUE);
        gtk_window_present(GTK_WINDOW(state->window));
        state->popup_visible = true;
    }
    return window_arm_timeout(state);
}

// Queries system volume and redraws widgets from fresh sample
static bool window_refresh_from_system(WindowState *state) {
    if (!osd_system_volume_query(&state->current_volume, stderr)) {
        window_set_error(state, "Failed to query system volume from wpctl");
        return false;
    }

    // Render follows successful query to keep state and UI aligned
    window_update_widgets(state);
    return true;
}

// Poll callback for watch mode updates and popup refresh
static gboolean window_on_watch_poll(gpointer user_data) {
    WindowState *state = user_data;
    OSDVolumeState sampled;

    // One shot timer model clears id then re schedules
    state->watch_source_id = 0U;
    if (!osd_system_volume_query(&sampled, stderr)) {
        window_log_watch_query_failure(
            state,
            "Failed to query system volume while watching; keeping watcher alive and retrying"
        );
        if (!window_schedule_watch_poll(state)) {
            return G_SOURCE_REMOVE;
        }
        return G_SOURCE_REMOVE;
    }
    window_log_watch_query_recovery(state);

    if (!state->has_previous_watch_sample) {
        // First good sample initializes baseline state
        state->current_volume = sampled;
        state->has_previous_watch_sample = true;
        window_update_widgets(state);
        if (!window_schedule_watch_poll(state)) {
            return G_SOURCE_REMOVE;
        }
        return G_SOURCE_REMOVE;
    }

    if (!volume_states_equal(&sampled, &state->current_volume)) {
        // Changed values trigger redraw and popup refresh
        state->current_volume = sampled;
        window_update_widgets(state);
        if (!window_show_popup(state)) {
            return G_SOURCE_REMOVE;
        }
    }

    if (!window_schedule_watch_poll(state)) {
        return G_SOURCE_REMOVE;
    }

    return G_SOURCE_REMOVE;
}

// Starts watch mode and keeps process alive for background polling
bool window_activate_watch_mode(WindowState *state, GtkApplication *app) {
    OSDVolumeState sampled;

    // Initial query may fail during startup races retry loop handles recovery
    if (osd_system_volume_query(&sampled, stderr)) {
        state->current_volume = sampled;
        state->has_previous_watch_sample = true;
        window_log_watch_query_recovery(state);
        window_update_widgets(state);

        if (!window_show_popup(state)) {
            return false;
        }
    } else {
        window_log_watch_query_failure(
            state,
            "Initial system volume query failed; watch mode will retry in background"
        );
        state->has_previous_watch_sample = false;
    }

    g_application_hold(G_APPLICATION(app));
    // Hold prevents GTK exit while popup is hidden in watch mode
    state->app_held = true;

    if (!window_schedule_watch_poll(state)) {
        return false;
    }

    return true;
}

// Runs single popup mode from system or explicit argument values
bool window_activate_single_popup(WindowState *state) {
    if (state->args.use_system_volume) {
        // System backed path queries wpctl before drawing
        if (!window_refresh_from_system(state)) {
            return false;
        }
    } else {
        // Manual path uses already parsed args volume fields
        state->current_volume = state->args.volume;
        window_update_widgets(state);
    }

    return window_show_popup(state);
}
