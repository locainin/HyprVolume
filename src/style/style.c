#include "style/style.h"

#include <errno.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define OSD_CUSTOM_CSS_MAX_BYTES (256U * 1024U)

typedef struct {
    const char *path;
    FILE *err_stream;
    bool has_parsing_error;
} OSDCustomCssParseContext;

static void osd_style_on_parsing_error(
    GtkCssProvider *provider,
    GtkCssSection *section,
    GError *error,
    gpointer user_data
) {
    OSDCustomCssParseContext *context = user_data;
    gchar *section_text = NULL;

    (void)provider;
    if (context == NULL || context->err_stream == NULL || context->path == NULL || error == NULL) {
        return;
    }
    context->has_parsing_error = true;

    if (section != NULL) {
        section_text = gtk_css_section_to_string(section);
    }

    if (section_text != NULL) {
        (void)fprintf(
            context->err_stream,
            "Custom CSS parsing error in %s at %s: %s\n",
            context->path,
            section_text,
            error->message
        );
        g_free(section_text);
        return;
    }

    (void)fprintf(context->err_stream, "Custom CSS parsing error in %s: %s\n", context->path, error->message);
}

/* Builds a CSS provider from runtime theme values. */
GtkCssProvider *osd_style_build_provider(const OSDTheme *theme) {
    GtkCssProvider *provider = NULL;
    gchar *css = NULL;
    unsigned int bar_width_px = 0U;
    unsigned int vertical_bar_height_px = 0U;
    unsigned int vertical_bar_width_px = 0U;
    unsigned int icon_wrap_px = 0U;
    unsigned int slash_font_px = 0U;

    if (theme == NULL) {
        return NULL;
    }

    /* Preserve icon/text spacing while letting the progress bar scale with width. */
    bar_width_px = (theme->width_px > 240U) ? theme->width_px - 240U : 80U;
    vertical_bar_height_px = (theme->height_px > 100U) ? theme->height_px - 70U : 56U;
    vertical_bar_width_px = (theme->width_px > 40U) ? theme->width_px / 3U : 12U;
    icon_wrap_px = theme->icon_size_px + 13U;
    slash_font_px = theme->icon_size_px + 2U;

    css = g_strdup_printf(
        ".osd-window {"
        "  background-color: transparent;"
        "  outline: none;"
        "}"
        ".osd-card {"
        "  min-width: %upx;"
        "  min-height: %upx;"
        "  padding: 7px 11px;"
        "  border-radius: %upx;"
        "  border: 1px solid %s;"
        "  background: %s;"
        "  box-shadow: 0 6px 18px rgba(7, 12, 22, 0.62);"
        "}"
        ".osd-icon-wrap {"
        "  min-width: %upx;"
        "  min-height: %upx;"
        "  margin-right: 10px;"
        "  border-radius: 7px;"
        "  background: linear-gradient(180deg, rgba(26, 37, 58, 0.98), rgba(20, 30, 46, 0.98));"
        "}"
        ".osd-icon {"
        "  color: %s;"
        "}"
        ".osd-icon-slash {"
        "  color: #ff4757;"
        "  font-size: %upx;"
        "  font-weight: 900;"
        "  line-height: 1;"
        "  margin-left: 1px;"
        "  margin-top: -1px;"
        "}"
        ".osd-bar {"
        "  min-width: %upx;"
        "  min-height: 11px;"
        "}"
        ".osd-bar trough {"
        "  min-height: 8px;"
        "  border-radius: 999px;"
        "  background: %s;"
        "  border: 1px solid rgba(95, 123, 169, 0.48);"
        "}"
        ".osd-bar progress {"
        "  min-height: 8px;"
        "  border-radius: 999px;"
        "  background: %s;"
        "}"
        ".osd-percent {"
        "  color: %s;"
        "  margin-left: 10px;"
        "  min-width: 42px;"
        "  font-size: %upx;"
        "  font-weight: 800;"
        "  letter-spacing: 0.2px;"
        "}"
        ".osd-percent.muted {"
        "  color: #d53a4f;"
        "}"
        ".osd-card.vertical {"
        "  padding: 10px 8px;"
        "}"
        ".osd-icon-wrap.vertical {"
        "  margin-right: 0;"
        "  margin-bottom: 9px;"
        "}"
        ".osd-bar.vertical {"
        "  min-width: %upx;"
        "  min-height: %upx;"
        "}"
        ".osd-bar.vertical trough {"
        "  min-width: %upx;"
        "  min-height: %upx;"
        "}"
        ".osd-bar.vertical progress {"
        "  min-width: %upx;"
        "  min-height: 0;"
        "}"
        ".osd-percent.vertical {"
        "  margin-left: 0;"
        "  margin-top: 9px;"
        "  min-width: 0;"
        "}",
        theme->width_px,
        theme->height_px,
        theme->corner_radius_px,
        theme->border_color,
        theme->background_color,
        icon_wrap_px,
        icon_wrap_px,
        theme->icon_color,
        slash_font_px,
        bar_width_px,
        theme->track_color,
        theme->fill_color,
        theme->text_color,
        theme->font_size_px,
        vertical_bar_width_px,
        vertical_bar_height_px,
        vertical_bar_width_px,
        vertical_bar_height_px,
        vertical_bar_width_px
    );
    if (css == NULL) {
        return NULL;
    }

    provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider, css);
    g_free(css);

    return provider;
}

/* Loads a user-provided custom CSS file with bounds and parsing checks. */
bool osd_style_build_custom_provider(const char *path, GtkCssProvider **out_provider, FILE *err_stream) {
    struct stat file_stat;
    FILE *css_file = NULL;
    char *css_text = NULL;
    GtkCssProvider *provider = NULL;
    OSDCustomCssParseContext context;
    gulong handler_id = 0UL;
    size_t file_size = 0U;
    size_t bytes_read = 0U;

    if (path == NULL || out_provider == NULL || err_stream == NULL) {
        return false;
    }
    if (*out_provider != NULL) {
        (void)fprintf(err_stream, "Custom CSS output slot must be NULL before loading\n");
        return false;
    }
    *out_provider = NULL;

    if (stat(path, &file_stat) != 0) {
        (void)fprintf(err_stream, "Failed to stat custom CSS file %s: %s\n", path, strerror(errno));
        return false;
    }
    if (!S_ISREG(file_stat.st_mode)) {
        (void)fprintf(err_stream, "Custom CSS path is not a regular file: %s\n", path);
        return false;
    }
    if (file_stat.st_size <= 0 || file_stat.st_size > (off_t)OSD_CUSTOM_CSS_MAX_BYTES) {
        (void)fprintf(
            err_stream,
            "Custom CSS file size must be in [1, %u] bytes, got %lld for %s\n",
            OSD_CUSTOM_CSS_MAX_BYTES,
            (long long)file_stat.st_size,
            path
        );
        return false;
    }

    file_size = (size_t)file_stat.st_size;
    css_text = calloc(file_size + 1U, sizeof(char));
    if (css_text == NULL) {
        (void)fprintf(err_stream, "Failed to allocate memory for custom CSS file %s\n", path);
        return false;
    }

    css_file = fopen(path, "rb");
    if (css_file == NULL) {
        (void)fprintf(err_stream, "Failed to open custom CSS file %s: %s\n", path, strerror(errno));
        free(css_text);
        return false;
    }

    bytes_read = fread(css_text, 1U, file_size, css_file);
    (void)fclose(css_file);
    if (bytes_read != file_size) {
        (void)fprintf(err_stream, "Failed to fully read custom CSS file %s\n", path);
        free(css_text);
        return false;
    }

    if (!g_utf8_validate(css_text, (gssize)file_size, NULL)) {
        (void)fprintf(err_stream, "Custom CSS file must be valid UTF-8: %s\n", path);
        free(css_text);
        return false;
    }

    provider = gtk_css_provider_new();
    context.path = path;
    context.err_stream = err_stream;
    context.has_parsing_error = false;
    handler_id = g_signal_connect(provider, "parsing-error", G_CALLBACK(osd_style_on_parsing_error), &context);
    gtk_css_provider_load_from_string(provider, css_text);
    if (handler_id != 0UL) {
        g_signal_handler_disconnect(provider, handler_id);
    }
    free(css_text);

    if (context.has_parsing_error) {
        g_object_unref(provider);
        return false;
    }

    *out_provider = provider;
    return true;
}

/* Symbolic icon thresholds mirror the common desktop volume buckets. */
const char *osd_style_icon_name_for_state(const OSDVolumeState *state) {
    if (state == NULL) {
        return "audio-volume-muted-symbolic";
    }

    if (state->muted || state->volume_percent <= 0) {
        return "audio-volume-muted-symbolic";
    }

    if (state->volume_percent < 34) {
        return "audio-volume-low-symbolic";
    }

    if (state->volume_percent < 67) {
        return "audio-volume-medium-symbolic";
    }

    return "audio-volume-high-symbolic";
}
