#include "internal.h"

#include "style/style.h"

#include <glib.h>
#include <stddef.h>

// Applies muted state classes and slash visibility in one place
static void window_apply_muted_css(WindowState *state, bool is_muted) {
    g_return_if_fail(state != NULL);
    g_return_if_fail(state->percent_label != NULL);

    if (state->muted_slash != NULL) {
        // Slash stays hidden until mute is active
        gtk_widget_set_visible(state->muted_slash, is_muted);
    }

    if (is_muted) {
        // Muted class drives label color from CSS
        gtk_widget_add_css_class(state->percent_label, "muted");
        return;
    }

    gtk_widget_remove_css_class(state->percent_label, "muted");
}

// Writes percent label text using bounded formatting and explicit truncation checks
static void window_format_percent_text(char *out_text, size_t out_text_size, bool is_muted, int clamped_percent) {
    int written = 0;

    if (out_text == NULL || out_text_size == 0U) {
        return;
    }

    if (is_muted) {
        written = g_snprintf(out_text, out_text_size, "MUTED");
    } else {
        written = g_snprintf(out_text, out_text_size, "%d%%", clamped_percent);
    }

    if (written < 0) {
        out_text[0] = '\0';
        return;
    }

    if ((size_t)written >= out_text_size) {
        out_text[out_text_size - 1U] = '\0';
    }
}

// Syncs icon, bar, and text from current sampled volume state
void window_update_widgets(WindowState *state) {
    char percent_text[16];
    const char *icon_name = NULL;
    double fraction = 0.0;
    int clamped_percent = 0;
    OSDVolumeState normalized_volume;
    bool is_muted = false;

    g_return_if_fail(state != NULL);
    g_return_if_fail(state->icon_image != NULL);
    g_return_if_fail(state->progress_bar != NULL);
    g_return_if_fail(state->percent_label != NULL);

    clamped_percent = state->current_volume.volume_percent;
    is_muted = state->current_volume.muted;

    if (clamped_percent < 0) {
        clamped_percent = 0;
    }
    if (clamped_percent > 200) {
        clamped_percent = 200;
    }

    normalized_volume = state->current_volume;
    normalized_volume.volume_percent = clamped_percent;

    icon_name = osd_style_icon_name_for_state(&normalized_volume);
    // Icon selection uses normalized volume range for stable buckets
    gtk_image_set_from_icon_name(GTK_IMAGE(state->icon_image), icon_name);

    fraction = (double)clamped_percent / 100.0;
    // Muted view shows status text instead of percent
    window_format_percent_text(percent_text, sizeof(percent_text), is_muted, clamped_percent);

    if (fraction < 0.0) {
        fraction = 0.0;
    }
    if (fraction > 1.0) {
        // Progress bar supports normalized range only
        fraction = 1.0;
    }

    window_apply_muted_css(state, is_muted);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(state->progress_bar), fraction);
    gtk_label_set_text(GTK_LABEL(state->percent_label), percent_text);
}
