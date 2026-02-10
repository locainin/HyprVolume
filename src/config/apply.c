#include "config/apply.h"

#include "config/error.h"
#include "config/json/json_config_fields.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

// Parses an optional bounded unsigned value from a named key
static bool parse_ranged_uint_from_key(
    const char *json_text,
    const char *key,
    unsigned int min_value,
    unsigned int max_value,
    unsigned int *target,
    FILE *err_stream
) {
    long value = 0L;

    if (!osd_config_parse_long_value(json_text, key, &value)) {
        if (osd_config_find_key(json_text, key) != NULL) {
            osd_config_write_error_value_message(err_stream, "Config value '", key, "' must be a valid integer\n");
            return false;
        }
        return true;
    }

    if (value < (long)min_value || value > (long)max_value) {
        osd_config_write_error_value_message(err_stream, "Config value '", key, "' is outside the allowed range\n");
        return false;
    }

    *target = (unsigned int)value;
    return true;
}

// Parses an optional bounded signed value from a named key
static bool parse_ranged_int_from_key(
    const char *json_text,
    const char *key,
    int min_value,
    int max_value,
    int *target,
    FILE *err_stream
) {
    long value = 0L;

    if (!osd_config_parse_long_value(json_text, key, &value)) {
        if (osd_config_find_key(json_text, key) != NULL) {
            osd_config_write_error_value_message(err_stream, "Config value '", key, "' must be a valid integer\n");
            return false;
        }
        return true;
    }

    if (value < (long)min_value || value > (long)max_value) {
        osd_config_write_error_value_message(err_stream, "Config value '", key, "' is outside the allowed range\n");
        return false;
    }

    *target = (int)value;
    return true;
}

// Maps string anchor values to the internal enum
static bool parse_anchor_value(const char *json_text, OSDAnchor *target, FILE *err_stream) {
    char value[32];
    bool key_present = osd_config_find_key(json_text, "anchor") != NULL;

    if (!osd_config_parse_string_value(json_text, "anchor", value, sizeof(value))) {
        if (key_present) {
            osd_config_write_error_text(err_stream, "Config value 'anchor' must be a string\n");
            return false;
        }
        return true;
    }

    if (strcmp(value, "top-center") == 0) {
        *target = OSD_ANCHOR_TOP_CENTER;
        return true;
    }
    if (strcmp(value, "top-left") == 0) {
        *target = OSD_ANCHOR_TOP_LEFT;
        return true;
    }
    if (strcmp(value, "top-right") == 0) {
        *target = OSD_ANCHOR_TOP_RIGHT;
        return true;
    }
    if (strcmp(value, "bottom-center") == 0) {
        *target = OSD_ANCHOR_BOTTOM_CENTER;
        return true;
    }
    if (strcmp(value, "bottom-left") == 0) {
        *target = OSD_ANCHOR_BOTTOM_LEFT;
        return true;
    }
    if (strcmp(value, "bottom-right") == 0) {
        *target = OSD_ANCHOR_BOTTOM_RIGHT;
        return true;
    }

    osd_config_write_error_value_message(err_stream, "Invalid config value for 'anchor': ", value, "\n");
    return false;
}

// Parses optional CSS values and rejects empty values
static bool parse_color_value(const char *json_text, const char *key, char *target, size_t target_size, FILE *err_stream) {
    bool key_present = osd_config_find_key(json_text, key) != NULL;

    if (!osd_config_parse_string_value(json_text, key, target, target_size)) {
        if (key_present) {
            osd_config_write_error_value_message(err_stream, "Config value '", key, "' must be a non-empty string\n");
            return false;
        }
        return true;
    }

    if (target[0] == '\0') {
        osd_config_write_error_value_message(err_stream, "Config value '", key, "' cannot be empty\n");
        return false;
    }

    return true;
}

// Parses css_file and updates css path state
static bool parse_css_file_value(const char *json_text, OSDArgs *args, FILE *err_stream) {
    char value[OSD_CONFIG_PATH_MAX];
    bool key_present = osd_config_find_key(json_text, "css_file") != NULL;
    size_t value_len = 0U;

    if (!osd_config_parse_string_value(json_text, "css_file", value, sizeof(value))) {
        if (key_present) {
            osd_config_write_error_text(err_stream, "Config value 'css_file' must be a string\n");
            return false;
        }
        return true;
    }

    value_len = strlen(value);
    if (value_len == 0U) {
        args->css_path[0] = '\0';
        args->css_path_set = false;
        return true;
    }

    if (value_len >= sizeof(args->css_path)) {
        osd_config_write_error_text(err_stream, "Config value 'css_file' is too long\n");
        return false;
    }

    if (!osd_config_copy_cstr_bounded(args->css_path, sizeof(args->css_path), value)) {
        osd_config_write_error_text(err_stream, "Config value 'css_file' is too long\n");
        return false;
    }

    args->css_path_set = true;
    return true;
}

// Applies numeric keys that share CLI-equivalent ranges
bool osd_config_apply_numeric_values(const char *json_text, OSDArgs *args, FILE *err_stream) {
    bool ok = true;

    ok &= parse_ranged_uint_from_key(json_text, "timeout_ms", 100U, 10000U, &args->timeout_ms, err_stream);
    ok &= parse_ranged_uint_from_key(json_text, "watch_poll_ms", 40U, 2000U, &args->watch_poll_ms, err_stream);
    ok &= parse_ranged_uint_from_key(json_text, "width", 40U, 1400U, &args->theme.width_px, err_stream);
    ok &= parse_ranged_uint_from_key(json_text, "height", 20U, 300U, &args->theme.height_px, err_stream);
    ok &= parse_ranged_uint_from_key(json_text, "margin_x", 0U, 500U, &args->theme.margin_x_px, err_stream);
    ok &= parse_ranged_uint_from_key(json_text, "margin_y", 0U, 500U, &args->theme.margin_y_px, err_stream);
    ok &= parse_ranged_int_from_key(json_text, "x_percent", 0, 100, &args->theme.x_percent, err_stream);
    ok &= parse_ranged_int_from_key(json_text, "y_percent", 0, 100, &args->theme.y_percent, err_stream);
    ok &= parse_ranged_uint_from_key(json_text, "radius", 0U, 200U, &args->theme.corner_radius_px, err_stream);
    ok &= parse_ranged_uint_from_key(json_text, "icon_size", 8U, 200U, &args->theme.icon_size_px, err_stream);
    ok &= parse_ranged_uint_from_key(json_text, "font_size", 8U, 200U, &args->theme.font_size_px, err_stream);

    return ok;
}

// Applies monitor index and sentinel handling
bool osd_config_apply_monitor_index(const char *json_text, OSDArgs *args, FILE *err_stream) {
    long monitor_value = 0L;

    if (!osd_config_parse_long_value(json_text, "monitor_index", &monitor_value)) {
        if (osd_config_find_key(json_text, "monitor_index") != NULL) {
            osd_config_write_error_text(err_stream, "Config value 'monitor_index' must be a valid integer\n");
            return false;
        }
        return true;
    }

    if (monitor_value < -1L || monitor_value > (long)INT_MAX) {
        osd_config_write_error_text(err_stream, "Config value 'monitor_index' is outside the allowed range\n");
        return false;
    }

    args->monitor_index = (int)monitor_value;
    return true;
}

// Applies boolean toggles when their keys are present
bool osd_config_apply_bool_values(const char *json_text, OSDArgs *args, FILE *err_stream) {
    bool bool_value = false;
    const char *watch_key = osd_config_find_key(json_text, "watch_mode");
    const char *system_key = osd_config_find_key(json_text, "use_system_volume");
    const char *css_replace_key = osd_config_find_key(json_text, "css_replace");
    const char *vertical_key = osd_config_find_key(json_text, "vertical");

    if (watch_key != NULL && !osd_config_parse_bool_value(json_text, "watch_mode", &bool_value)) {
        osd_config_write_error_text(err_stream, "Config value 'watch_mode' must be true or false\n");
        return false;
    }
    if (watch_key != NULL) {
        args->watch_mode = bool_value;
    }

    if (system_key != NULL && !osd_config_parse_bool_value(json_text, "use_system_volume", &bool_value)) {
        osd_config_write_error_text(err_stream, "Config value 'use_system_volume' must be true or false\n");
        return false;
    }
    if (system_key != NULL) {
        args->use_system_volume = bool_value;
    }

    if (css_replace_key != NULL && !osd_config_parse_bool_value(json_text, "css_replace", &bool_value)) {
        osd_config_write_error_text(err_stream, "Config value 'css_replace' must be true or false\n");
        return false;
    }
    if (css_replace_key != NULL) {
        args->css_replace = bool_value;
    }

    if (vertical_key != NULL && !osd_config_parse_bool_value(json_text, "vertical", &bool_value)) {
        osd_config_write_error_text(err_stream, "Config value 'vertical' must be true or false\n");
        return false;
    }
    if (vertical_key != NULL) {
        args->theme.vertical_layout = bool_value;
    }

    return true;
}

// Applies visual styling keys independently
bool osd_config_apply_visual_values(const char *json_text, OSDArgs *args, FILE *err_stream) {
    bool ok = true;

    ok &= parse_anchor_value(json_text, &args->theme.anchor, err_stream);
    ok &= parse_color_value(json_text, "background_color", args->theme.background_color, sizeof(args->theme.background_color), err_stream);
    ok &= parse_color_value(json_text, "border_color", args->theme.border_color, sizeof(args->theme.border_color), err_stream);
    ok &= parse_color_value(json_text, "fill_color", args->theme.fill_color, sizeof(args->theme.fill_color), err_stream);
    ok &= parse_color_value(json_text, "track_color", args->theme.track_color, sizeof(args->theme.track_color), err_stream);
    ok &= parse_color_value(json_text, "text_color", args->theme.text_color, sizeof(args->theme.text_color), err_stream);
    ok &= parse_color_value(json_text, "icon_color", args->theme.icon_color, sizeof(args->theme.icon_color), err_stream);
    ok &= parse_css_file_value(json_text, args, err_stream);

    return ok;
}
