#include "config/schema.h"

#include "config/json/json_config_fields.h"

#include <stddef.h>

// Maintains the authoritative allowlist for top-level config keys
static const char *const OSD_ALLOWED_KEYS[] = {
    "watch_mode",
    "use_system_volume",
    "enable_slide",
    "css_file",
    "css_replace",
    "timeout_ms",
    "watch_poll_ms",
    "monitor_index",
    "anchor",
    "x_percent",
    "y_percent",
    "margin_x",
    "margin_y",
    "width",
    "height",
    "vertical",
    "radius",
    "icon_size",
    "font_size",
    "background_color",
    "border_color",
    "fill_color",
    "track_color",
    "text_color",
    "icon_color"
};

// Validates top-level keys against the schema allowlist
bool osd_config_schema_validate_top_level_keys(const char *json_text, FILE *err_stream) {
    return osd_config_validate_top_level_keys(
        json_text,
        OSD_ALLOWED_KEYS,
        sizeof(OSD_ALLOWED_KEYS) / sizeof(OSD_ALLOWED_KEYS[0]),
        err_stream
    );
}
