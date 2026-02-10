#include "window/window.h"

#include "internal.h"
#include "style/style.h"
#include "system/volume.h"

#include <stdio.h>
#include <string.h>

#define OSD_MIN_IDLE_WATCH_POLL_MS 250U
#define OSD_MAX_IDLE_WATCH_POLL_MS 1000U

static gboolean window_on_watch_poll(gpointer user_data);
static void window_set_error(WindowState *state, const char *message);
static void window_log_watch_query_failure(WindowState *state, const char *message);
static void window_log_watch_query_recovery(WindowState *state);

/*
 * Derives a conservative idle poll interval to keep watch-mode CPU lower when
 * the popup is hidden, while preserving responsive active updates.
 */
static unsigned int window_compute_idle_watch_poll_ms(unsigned int active_poll_ms) {
    unsigned int idle_poll_ms = OSD_MAX_IDLE_WATCH_POLL_MS;

    if (active_poll_ms <= (OSD_MAX_IDLE_WATCH_POLL_MS / 3U)) {
        /* Hidden polling is slower to reduce wakeups while preserving response. */
        idle_poll_ms = active_poll_ms * 3U;
    }

    if (idle_poll_ms < OSD_MIN_IDLE_WATCH_POLL_MS) {
        /* Guardrail prevents excessively hot loops on tiny poll intervals. */
        idle_poll_ms = OSD_MIN_IDLE_WATCH_POLL_MS;
    }
    if (idle_poll_ms > OSD_MAX_IDLE_WATCH_POLL_MS) {
        /* Guardrail keeps hidden-state responsiveness within expected bounds. */
        idle_poll_ms = OSD_MAX_IDLE_WATCH_POLL_MS;
    }

    return idle_poll_ms;
}

/* Arms the one-shot watch timer using active or idle interval. */
static bool window_schedule_watch_poll(WindowState *state) {
    unsigned int next_poll_ms = 0U;

    if (state == NULL) {
        return false;
    }
    if (state->watch_source_id != 0U) {
        /* One-shot scheduling removes stale timers before re-arming. */
        g_source_remove(state->watch_source_id);
        state->watch_source_id = 0U;
    }

    /* Visible popups poll at active rate, hidden popups at idle rate. */
    next_poll_ms = state->popup_visible ? state->args.watch_poll_ms : state->watch_idle_poll_ms;
    state->watch_source_id = g_timeout_add(next_poll_ms, window_on_watch_poll, state);
    if (state->watch_source_id == 0U) {
        window_set_error(state, "Failed to start watch timer");
        return false;
    }

    return true;
}

/* Compares two volume samples to suppress redundant redraws. */
static bool volume_states_equal(const OSDVolumeState *a, const OSDVolumeState *b) {
    return a->volume_percent == b->volume_percent && a->muted == b->muted;
}

/* Records a fatal runtime error and requests application shutdown. */
static void window_set_error(WindowState *state, const char *message) {
    if (message != NULL) {
        g_printerr("%s\n", message);
    }

    /* Non-zero exit code is preserved even if GTK exits normally. */
    state->exit_code = 1;
    if (g_application_get_default() != NULL) {
        g_application_quit(g_application_get_default());
    }
}

/*
 * Logs repeated watch-mode query failures once until a successful query resets
 * the state. This preserves diagnostics without flooding stderr every poll.
 */
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

/* Emits a recovery marker once after transient system query failures clear. */
static void window_log_watch_query_recovery(WindowState *state) {
    if (state == NULL || !state->watch_query_failure_logged) {
        return;
    }

    g_printerr("System volume query recovered; watch mode resumed\n");
    state->watch_query_failure_logged = false;
}

/* Applies muted-state CSS classes to all widgets affected by visual state. */
static void window_apply_muted_css(WindowState *state, bool is_muted) {
    if (state->muted_slash != NULL) {
        /* Slash overlay remains hidden for non-muted states. */
        gtk_widget_set_visible(state->muted_slash, is_muted);
    }

    if (is_muted) {
        /* Label style class drives muted coloring in CSS theme. */
        gtk_widget_add_css_class(state->percent_label, "muted");
        return;
    }

    gtk_widget_remove_css_class(state->percent_label, "muted");
}

/* Synchronizes icon, progress bar, and label with the current volume sample. */
static void window_update_widgets(WindowState *state) {
    char percent_text[16];
    const char *icon_name = NULL;
    double fraction = 0.0;
    int clamped_percent = state->current_volume.volume_percent;
    bool is_muted = state->current_volume.muted;

    icon_name = osd_style_icon_name_for_state(&state->current_volume);
    /* Symbolic icon choice reflects bucketed volume thresholds. */
    gtk_image_set_from_icon_name(GTK_IMAGE(state->icon_image), icon_name);

    if (clamped_percent < 0) {
        clamped_percent = 0;
    }
    if (clamped_percent > 200) {
        clamped_percent = 200;
    }

    fraction = (double)clamped_percent / 100.0;
    if (is_muted) {
        /* Muted mode shows explicit status text rather than percentage. */
        (void)snprintf(percent_text, sizeof(percent_text), "MUTED");
    } else {
        (void)snprintf(percent_text, sizeof(percent_text), "%d%%", clamped_percent);
    }
    if (fraction < 0.0) {
        fraction = 0.0;
    }
    if (fraction > 1.0) {
        /* Progress bar represents normalized 0..1 range only. */
        fraction = 1.0;
    }

    window_apply_muted_css(state, is_muted);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(state->progress_bar), fraction);
    gtk_label_set_text(GTK_LABEL(state->percent_label), percent_text);
}

/* Handles auto-hide expiration for one-shot and watch modes. */
static gboolean window_on_timeout(gpointer user_data) {
    WindowState *state = user_data;

    /* Timeout source is one-shot; clear stored source id first. */
    state->timeout_source_id = 0U;
    if (state->args.watch_mode) {
        /* Watch mode hides popup but keeps process and poll loop alive. */
        state->popup_visible = false;
        gtk_widget_set_visible(state->window, FALSE);
        return G_SOURCE_REMOVE;
    }

    if (g_application_get_default() != NULL) {
        g_application_quit(g_application_get_default());
    }

    return G_SOURCE_REMOVE;
}

/* Restarts the hide timer whenever the popup is shown or refreshed. */
static bool window_arm_timeout(WindowState *state) {
    if (state->timeout_source_id != 0U) {
        /* Re-arming timeout prevents stale hide events from firing later. */
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

/* Presents the popup and arms hide behavior according to runtime mode. */
static bool window_show_popup(WindowState *state) {
    if (!state->popup_visible) {
        /* Avoid repeated present() calls while popup is already visible. */
        gtk_widget_set_visible(state->window, TRUE);
        gtk_window_present(GTK_WINDOW(state->window));
        state->popup_visible = true;
    }
    return window_arm_timeout(state);
}

/* Pulls the latest system sink volume and updates all visible controls. */
static bool window_refresh_from_system(WindowState *state) {
    if (!osd_system_volume_query(&state->current_volume, stderr)) {
        window_set_error(state, "Failed to query system volume from wpctl");
        return false;
    }

    /* Render follows successful sample to keep UI and state synchronized. */
    window_update_widgets(state);
    return true;
}

/* Periodic watch callback that only shows the popup when values change. */
static gboolean window_on_watch_poll(gpointer user_data) {
    WindowState *state = user_data;
    OSDVolumeState sampled;

    /* One-shot timer model requires explicit re-arm on each callback. */
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
        /* First sample initializes baseline without forcing another show. */
        state->current_volume = sampled;
        state->has_previous_watch_sample = true;
        window_update_widgets(state);
        if (!window_schedule_watch_poll(state)) {
            return G_SOURCE_REMOVE;
        }
        return G_SOURCE_REMOVE;
    }

    if (!volume_states_equal(&sampled, &state->current_volume)) {
        /* Only changed values trigger redraw/popup refresh behavior. */
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

/* Releases timers, CSS providers, and held application references. */
static void window_cleanup(WindowState *state) {
    if (state->timeout_source_id != 0U) {
        /* Remove timeout source to avoid callbacks after shutdown. */
        g_source_remove(state->timeout_source_id);
        state->timeout_source_id = 0U;
    }

    if (state->watch_source_id != 0U) {
        /* Remove watch source to avoid callbacks after shutdown. */
        g_source_remove(state->watch_source_id);
        state->watch_source_id = 0U;
    }

    if (state->app_held && g_application_get_default() != NULL) {
        /* Release hold acquired in watch mode activation. */
        g_application_release(g_application_get_default());
        state->app_held = false;
    }

    if (state->css_provider != NULL && state->window != NULL) {
        GdkDisplay *display = gtk_widget_get_display(state->window);
        if (display != NULL) {
            /* Providers are display-scoped and must be removed explicitly. */
            gtk_style_context_remove_provider_for_display(display, GTK_STYLE_PROVIDER(state->css_provider));
        }
        g_object_unref(state->css_provider);
        state->css_provider = NULL;
    }

    if (state->custom_css_provider != NULL && state->window != NULL) {
        GdkDisplay *display = gtk_widget_get_display(state->window);
        if (display != NULL) {
            /* Providers are display-scoped and must be removed explicitly. */
            gtk_style_context_remove_provider_for_display(display, GTK_STYLE_PROVIDER(state->custom_css_provider));
        }
        g_object_unref(state->custom_css_provider);
        state->custom_css_provider = NULL;
    }
}

/* GTK shutdown callback used to guarantee resource cleanup. */
static void window_on_shutdown(GApplication *app, gpointer user_data) {
    WindowState *state = user_data;

    (void)app;
    window_cleanup(state);
}

/* Starts watch mode and keeps the process alive for future volume changes. */
static bool window_activate_watch_mode(WindowState *state, GtkApplication *app) {
    OSDVolumeState sampled;

    /*
     * Watch mode should survive transient PipeWire or wpctl startup races.
     * Initial query failures keep the process alive and retry in the poll loop.
     */
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
    /* Hold prevents GTK from exiting when popup auto-hides in watch mode. */
    state->app_held = true;

    if (!window_schedule_watch_poll(state)) {
        return false;
    }

    return true;
}

/* Runs single-popup mode using either manual or system-backed volume data. */
static bool window_activate_single_popup(WindowState *state) {
    if (state->args.use_system_volume) {
        /* Query live system state for one-shot display path. */
        if (!window_refresh_from_system(state)) {
            return false;
        }
    } else {
        /* Manual CLI/config values bypass wpctl query entirely. */
        state->current_volume = state->args.volume;
        window_update_widgets(state);
    }

    return window_show_popup(state);
}

/* Creates the base GTK window and attaches static widget structure. */
static void window_configure_base_window(WindowState *state, GtkApplication *app) {
    state->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(state->window), "HyprVolume");
    gtk_window_set_decorated(GTK_WINDOW(state->window), FALSE);
    gtk_window_set_resizable(GTK_WINDOW(state->window), FALSE);

    gtk_widget_add_css_class(state->window, "osd-window");
    /* Placement runs before widget tree to ensure namespace/anchor settings. */
    window_apply_placement(state);
    window_build_widgets(state);
}

/* Selects the runtime activation path based on parsed arguments. */
static void window_activate_mode(WindowState *state, GtkApplication *app) {
    if (state->args.watch_mode) {
        /* Watch mode is long-lived and timer-driven. */
        if (!window_activate_watch_mode(state, app) && state->exit_code == 0) {
            window_set_error(state, "Failed to activate watch mode");
        }
        return;
    }

    /* Single popup exits automatically after timeout elapses. */
    if (!window_activate_single_popup(state) && state->exit_code == 0) {
        window_set_error(state, "Failed to activate single-popup mode");
    }
}

/* GTK activate callback for window setup, CSS install, and mode startup. */
static void window_on_activate(GtkApplication *app, gpointer user_data) {
    WindowState *state = user_data;

    window_configure_base_window(state, app);

    if (!window_init_css(state)) {
        window_set_error(state, "Failed to initialize OSD CSS styling");
        return;
    }

    window_activate_mode(state, app);
}

/* Main GUI entrypoint used by app/main.c after argument resolution. */
int osd_window_run(const OSDArgs *args) {
    GtkApplication *app = NULL;
    WindowState state;
    int status = 1;

    if (args == NULL) {
        g_printerr("Internal error: missing OSD arguments\n");
        return 1;
    }

    (void)memset(&state, 0, sizeof(state));
    /* Full zero-init simplifies lifecycle cleanup paths and flags. */
    state.args = *args;
    state.current_volume = args->volume;
    state.popup_visible = false;
    /* Idle poll interval is derived from active poll interval once at start. */
    state.watch_idle_poll_ms = window_compute_idle_watch_poll_ms(args->watch_poll_ms);
    state.exit_code = 0;

    app = gtk_application_new("dev.hyprland.hyprvolume", G_APPLICATION_NON_UNIQUE);
    if (app == NULL) {
        g_printerr("Failed to create GTK application\n");
        return 1;
    }

    g_signal_connect(app, "activate", G_CALLBACK(window_on_activate), &state);
    g_signal_connect(app, "shutdown", G_CALLBACK(window_on_shutdown), &state);

    status = g_application_run(G_APPLICATION(app), 0, NULL);
    /* App object is unreferenced after main loop returns. */
    g_object_unref(app);

    if (state.exit_code != 0) {
        return state.exit_code;
    }

    return status;
}
