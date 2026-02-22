#include "window/window.h"

#include "internal.h"

#define OSD_MIN_IDLE_WATCH_POLL_MS 250U
#define OSD_MAX_IDLE_WATCH_POLL_MS 1000U

// Derives idle poll interval to keep hidden watch loops low cost
static unsigned int window_compute_idle_watch_poll_ms(unsigned int active_poll_ms) {
  unsigned int idle_poll_ms = OSD_MAX_IDLE_WATCH_POLL_MS;

  if (active_poll_ms <= (OSD_MAX_IDLE_WATCH_POLL_MS / 3U)) {
    // Hidden polling runs slower and still keeps watch mode responsive
    idle_poll_ms = active_poll_ms * 3U;
  }

  if (idle_poll_ms < OSD_MIN_IDLE_WATCH_POLL_MS) {
    // Lower guardrail prevents tight polling loops
    idle_poll_ms = OSD_MIN_IDLE_WATCH_POLL_MS;
  }
  if (idle_poll_ms > OSD_MAX_IDLE_WATCH_POLL_MS) {
    // Upper guardrail keeps wakeup latency bounded
    idle_poll_ms = OSD_MAX_IDLE_WATCH_POLL_MS;
  }

  return idle_poll_ms;
}

// Records fatal runtime errors and requests application shutdown
void window_set_error(WindowState *state, const char *message) {
  if (message != NULL) {
    g_printerr("%s\n", message);
  }

  // Non zero exit code is preserved even if GTK exits normally
  state->exit_code = 1;
  if (g_application_get_default() != NULL) {
    g_application_quit(g_application_get_default());
  }
}

// Releases timers CSS providers and held app references
static void window_cleanup(WindowState *state) {
  if (state->timeout_source_id != 0U) {
    // Remove timeout source before shutdown returns
    g_source_remove(state->timeout_source_id);
    state->timeout_source_id = 0U;
  }

  if (state->watch_source_id != 0U) {
    // Remove watch source before shutdown returns
    g_source_remove(state->watch_source_id);
    state->watch_source_id = 0U;
  }

  if (state->app_held && g_application_get_default() != NULL) {
    // Release hold acquired during watch activation
    g_application_release(g_application_get_default());
    state->app_held = false;
  }

  if (state->css_provider != NULL && state->window != NULL) {
    GdkDisplay *display = gtk_widget_get_display(state->window);
    if (display != NULL) {
      // Display scoped provider must be removed explicitly
      gtk_style_context_remove_provider_for_display(display, GTK_STYLE_PROVIDER(state->css_provider));
    }
    g_object_unref(state->css_provider);
    state->css_provider = NULL;
  }

  if (state->custom_css_provider != NULL && state->window != NULL) {
    GdkDisplay *display = gtk_widget_get_display(state->window);
    if (display != NULL) {
      // Display scoped provider must be removed explicitly
      gtk_style_context_remove_provider_for_display(display, GTK_STYLE_PROVIDER(state->custom_css_provider));
    }
    g_object_unref(state->custom_css_provider);
    state->custom_css_provider = NULL;
  }
}

// GTK shutdown callback for guaranteed cleanup
static void window_on_shutdown(GApplication *app, gpointer user_data) {
  WindowState *state = user_data;

  (void)app;
  window_cleanup(state);
}

// Creates base GTK window and attaches static widget structure
static void window_configure_base_window(WindowState *state, GtkApplication *app) {
  state->window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(state->window), "HyprVolume");
  gtk_window_set_decorated(GTK_WINDOW(state->window), FALSE);
  gtk_window_set_resizable(GTK_WINDOW(state->window), FALSE);

  gtk_widget_add_css_class(state->window, "osd-window");
  // Placement runs before widget tree for namespace and anchors
  window_apply_placement(state);
  window_build_widgets(state);
}

// Selects runtime activation path based on parsed args
static void window_activate_mode(WindowState *state, GtkApplication *app) {
  if (state->args.watch_mode) {
    // Watch mode runs as a long lived timer driven loop
    if (!window_activate_watch_mode(state, app) && state->exit_code == 0) {
      window_set_error(state, "Failed to activate watch mode");
    }
    return;
  }

  // Single popup exits after timeout elapses
  if (!window_activate_single_popup(state) && state->exit_code == 0) {
    window_set_error(state, "Failed to activate single-popup mode");
  }
}

// GTK activate callback for setup CSS and startup mode
static void window_on_activate(GtkApplication *app, gpointer user_data) {
  WindowState *state = user_data;

  window_configure_base_window(state, app);

  if (!window_init_css(state)) {
    window_set_error(state, "Failed to initialize OSD CSS styling");
    return;
  }

  window_activate_mode(state, app);
}

// Main GUI entrypoint used by app main after args parse
int osd_window_run(const OSDArgs *args) {
  GtkApplication *app = NULL;
  WindowState state = {0};
  int status = 1;

  if (args == NULL) {
    g_printerr("Internal error: missing OSD arguments\n");
    return 1;
  }

  // Zero init keeps cleanup paths and flags predictable
  state.args = *args;
  state.current_volume = args->volume;
  state.popup_visible = false;
  // Idle poll interval is derived once from active interval
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
  // App object is unreferenced after main loop returns
  g_object_unref(app);

  if (state.exit_code != 0) {
    return state.exit_code;
  }

  return status;
}
