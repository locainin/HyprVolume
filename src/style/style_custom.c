#include "style/style_custom.h"

#include "common/safeio.h"

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

// Emits parsing diagnostics from GTK CSS parser callback
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
        (void)osd_io_write_text(context->err_stream, "Custom CSS parsing error in ");
        (void)osd_io_write_text(context->err_stream, context->path);
        (void)osd_io_write_text(context->err_stream, " at ");
        (void)osd_io_write_text(context->err_stream, section_text);
        (void)osd_io_write_text(context->err_stream, ": ");
        (void)osd_io_write_line(context->err_stream, error->message);
        g_free(section_text);
        return;
    }

    (void)osd_io_write_text(context->err_stream, "Custom CSS parsing error in ");
    (void)osd_io_write_text(context->err_stream, context->path);
    (void)osd_io_write_text(context->err_stream, ": ");
    (void)osd_io_write_line(context->err_stream, error->message);
}

bool osd_style_custom_build_provider(const char *path, GtkCssProvider **out_provider, FILE *err_stream) {
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
        (void)osd_io_write_line(err_stream, "Custom CSS output slot must be NULL before loading");
        return false;
    }
    *out_provider = NULL;

    if (stat(path, &file_stat) != 0) {
        (void)osd_io_write_text(err_stream, "Failed to stat custom CSS file ");
        (void)osd_io_write_text(err_stream, path);
        (void)osd_io_write_text(err_stream, ": ");
        (void)osd_io_write_line(err_stream, strerror(errno));
        return false;
    }
    if (!S_ISREG(file_stat.st_mode)) {
        (void)osd_io_write_text(err_stream, "Custom CSS path is not a regular file: ");
        (void)osd_io_write_line(err_stream, path);
        return false;
    }
    if (file_stat.st_size <= 0 || file_stat.st_size > (off_t)OSD_CUSTOM_CSS_MAX_BYTES) {
        (void)osd_io_write_text(err_stream, "Custom CSS file size must be in [1, ");
        (void)osd_io_write_unsigned_long_long(err_stream, (unsigned long long)OSD_CUSTOM_CSS_MAX_BYTES);
        (void)osd_io_write_text(err_stream, "] bytes, got ");
        (void)osd_io_write_long_long(err_stream, (long long)file_stat.st_size);
        (void)osd_io_write_text(err_stream, " for ");
        (void)osd_io_write_line(err_stream, path);
        return false;
    }

    file_size = (size_t)file_stat.st_size;
    css_text = calloc(file_size + 1U, sizeof(char));
    if (css_text == NULL) {
        (void)osd_io_write_text(err_stream, "Failed to allocate memory for custom CSS file ");
        (void)osd_io_write_line(err_stream, path);
        return false;
    }

    css_file = fopen(path, "rb");
    if (css_file == NULL) {
        (void)osd_io_write_text(err_stream, "Failed to open custom CSS file ");
        (void)osd_io_write_text(err_stream, path);
        (void)osd_io_write_text(err_stream, ": ");
        (void)osd_io_write_line(err_stream, strerror(errno));
        free(css_text);
        return false;
    }

    bytes_read = fread(css_text, 1U, file_size, css_file);
    (void)fclose(css_file);
    if (bytes_read != file_size) {
        (void)osd_io_write_text(err_stream, "Failed to fully read custom CSS file ");
        (void)osd_io_write_line(err_stream, path);
        free(css_text);
        return false;
    }

    if (!g_utf8_validate(css_text, (gssize)file_size, NULL)) {
        (void)osd_io_write_text(err_stream, "Custom CSS file must be valid UTF-8: ");
        (void)osd_io_write_line(err_stream, path);
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
