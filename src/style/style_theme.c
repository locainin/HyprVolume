#include "style/style_theme.h"

#include <glib.h>

// Derives theme-dependent pixel dimensions used by generated CSS
static void compute_theme_geometry(
    const OSDTheme *theme,
    unsigned int *bar_width_px,
    unsigned int *vertical_bar_height_px,
    unsigned int *vertical_bar_width_px,
    unsigned int *icon_wrap_px,
    unsigned int *slash_font_px
) {
    if (theme == NULL ||
        bar_width_px == NULL ||
        vertical_bar_height_px == NULL ||
        vertical_bar_width_px == NULL ||
        icon_wrap_px == NULL ||
        slash_font_px == NULL) {
        return;
    }

    // Preserves icon and label spacing while scaling bar area with width
    *bar_width_px = (theme->width_px > 240U) ? theme->width_px - 240U : 80U;
    *vertical_bar_height_px = (theme->height_px > 100U) ? theme->height_px - 70U : 56U;
    *vertical_bar_width_px = (theme->width_px > 40U) ? theme->width_px / 3U : 12U;
    *icon_wrap_px = theme->icon_size_px + 13U;
    *slash_font_px = theme->icon_size_px + 2U;
}

GtkCssProvider *osd_style_theme_build_provider(const OSDTheme *theme) {
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

    compute_theme_geometry(
        theme,
        &bar_width_px,
        &vertical_bar_height_px,
        &vertical_bar_width_px,
        &icon_wrap_px,
        &slash_font_px
    );

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
