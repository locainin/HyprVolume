#include "internal.h"

#include "style/style.h"

// Builds the fixed widget hierarchy used for all runtime themes
void window_build_widgets(WindowState *state) {
    GtkWidget *container = NULL;
    GtkOrientation orientation = GTK_ORIENTATION_HORIZONTAL;
    bool is_vertical = false;

    is_vertical = state->args.theme.vertical_layout;
    if (is_vertical) {
        // Vertical mode rotates logical flow for compact side placement
        orientation = GTK_ORIENTATION_VERTICAL;
    }

    // Layout remains stable across themes with icon progress bar and label
    container = gtk_box_new(orientation, 0);
    // Card class owns border and background styling in CSS theme
    gtk_widget_add_css_class(container, "osd-card");
    if (is_vertical) {
        // Vertical modifier enables direction specific spacing rules
        gtk_widget_add_css_class(container, "vertical");
    }
    gtk_window_set_child(GTK_WINDOW(state->window), container);

    state->icon_overlay = gtk_overlay_new();
    // Overlay allows slash indicator to be layered over volume icon
    gtk_widget_set_valign(state->icon_overlay, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(state->icon_overlay, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class(state->icon_overlay, "osd-icon-wrap");
    if (is_vertical) {
        gtk_widget_add_css_class(state->icon_overlay, "vertical");
    }

    state->icon_image = gtk_image_new_from_icon_name("audio-volume-medium-symbolic");
    // Pixel size comes from theme config for predictable scaling
    gtk_image_set_pixel_size(GTK_IMAGE(state->icon_image), (int)state->args.theme.icon_size_px);
    gtk_widget_set_valign(state->icon_image, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(state->icon_image, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class(state->icon_image, "osd-icon");
    gtk_overlay_set_child(GTK_OVERLAY(state->icon_overlay), state->icon_image);

    state->muted_slash = gtk_label_new("/");
    // Slash overlay is toggled by runtime mute state updates
    gtk_widget_set_halign(state->muted_slash, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(state->muted_slash, GTK_ALIGN_CENTER);
    gtk_widget_set_visible(state->muted_slash, FALSE);
    gtk_widget_add_css_class(state->muted_slash, "osd-icon-slash");
    gtk_overlay_add_overlay(GTK_OVERLAY(state->icon_overlay), state->muted_slash);

    gtk_box_append(GTK_BOX(container), state->icon_overlay);

    state->progress_bar = gtk_progress_bar_new();
    // Progress bar orientation tracks overall layout orientation
    gtk_widget_set_valign(state->progress_bar, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(state->progress_bar, GTK_ALIGN_CENTER);
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(state->progress_bar), FALSE);
    gtk_orientable_set_orientation(GTK_ORIENTABLE(state->progress_bar), orientation);
    gtk_widget_set_hexpand(state->progress_bar, !is_vertical);
    gtk_widget_set_vexpand(state->progress_bar, is_vertical);
    gtk_widget_add_css_class(state->progress_bar, "osd-bar");
    if (is_vertical) {
        // Vertical modifier class switches trough and progress sizing CSS
        gtk_widget_add_css_class(state->progress_bar, "vertical");
    }
    gtk_box_append(GTK_BOX(container), state->progress_bar);

    state->percent_label = gtk_label_new("0%");
    // Label text is updated per volume sample in window_update_widgets
    gtk_widget_set_valign(state->percent_label, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(state->percent_label, GTK_ALIGN_CENTER);
    gtk_label_set_xalign(GTK_LABEL(state->percent_label), 0.5F);
    gtk_widget_add_css_class(state->percent_label, "osd-percent");
    if (is_vertical) {
        gtk_widget_add_css_class(state->percent_label, "vertical");
    }
    gtk_box_append(GTK_BOX(container), state->percent_label);
}

// Creates and installs a display scoped CSS provider for the active window
bool window_init_css(WindowState *state) {
    GdkDisplay *display = NULL;

    display = gtk_widget_get_display(state->window);
    if (display == NULL) {
        // Display must exist before adding display scoped providers
        return false;
    }

    if (!state->args.css_replace) {
        // Built in provider establishes baseline card and slider styling
        state->css_provider = osd_style_build_provider(&state->args.theme);
        if (state->css_provider == NULL) {
            return false;
        }

        gtk_style_context_add_provider_for_display(
            display,
            GTK_STYLE_PROVIDER(state->css_provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );
    }

    if (state->args.css_path_set) {
        // Optional custom provider is loaded after base provider
        if (!osd_style_build_custom_provider(state->args.css_path, &state->custom_css_provider, stderr)) {
            return false;
        }

        gtk_style_context_add_provider_for_display(
            display,
            GTK_STYLE_PROVIDER(state->custom_css_provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1U
        );
    }

    return true;
}
