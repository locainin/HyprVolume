#include "config/config.h"
#include "config/json/json_config_fields.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define OSD_CONFIG_MAX_SIZE_BYTES (1024L * 1024L)

/* Writes text to the error stream without printf-style formatting APIs. */
static void write_error_text(FILE *err_stream, const char *text) {
    size_t text_size = 0U;

    if (err_stream == NULL || text == NULL) {
        return;
    }
    text_size = strlen(text);
    if (text_size == 0U) {
        return;
    }
    (void)fwrite(text, sizeof(char), text_size, err_stream);
}

/* Writes prefix + value + suffix to keep call sites compact and readable. */
static void write_error_value_message(FILE *err_stream, const char *prefix, const char *value, const char *suffix) {
    write_error_text(err_stream, prefix);
    write_error_text(err_stream, value);
    write_error_text(err_stream, suffix);
}

/* Copies a C string into a fixed buffer and guarantees NUL termination. */
static bool copy_cstr_bounded(char *destination, size_t destination_size, const char *source) {
    size_t index = 0U;

    if (destination == NULL || source == NULL || destination_size == 0U) {
        return false;
    }
    while (source[index] != '\0' && (index + 1U) < destination_size) {
        destination[index] = source[index];
        ++index;
    }
    if (source[index] != '\0') {
        destination[0] = '\0';
        return false;
    }
    destination[index] = '\0';
    return true;
}

/* Parses a bounded unsigned integer field when present in the config file. */
static bool parse_ranged_uint_from_key(
    const char *json_text,
    const char *key,
    unsigned int min_value,
    unsigned int max_value,
    unsigned int *target,
    FILE *err_stream
) {
    long value = 0L;

    /* Absent keys are allowed so defaults can remain active. */
    if (!osd_config_parse_long_value(json_text, key, &value)) {
        if (osd_config_find_key(json_text, key) != NULL) {
            /* A present key with invalid syntax is a hard configuration error. */
            write_error_value_message(err_stream, "Config value '", key, "' must be a valid integer\n");
            return false;
        }
        return true;
    }

    /* Numeric range is enforced before narrowing to unsigned storage. */
    if (value < (long)min_value || value > (long)max_value) {
        write_error_value_message(err_stream, "Config value '", key, "' is outside the allowed range\n");
        return false;
    }

    /* Conversion is safe because bounds were validated above. */
    *target = (unsigned int)value;
    return true;
}

/* Parses a bounded signed integer field when present in the config file. */
static bool parse_ranged_int_from_key(
    const char *json_text,
    const char *key,
    int min_value,
    int max_value,
    int *target,
    FILE *err_stream
) {
    long value = 0L;

    /* Missing keys are non-fatal; only malformed present keys fail. */
    if (!osd_config_parse_long_value(json_text, key, &value)) {
        if (osd_config_find_key(json_text, key) != NULL) {
            /* This branch distinguishes parse failure from omitted setting. */
            write_error_value_message(err_stream, "Config value '", key, "' must be a valid integer\n");
            return false;
        }
        return true;
    }

    /* Signed range check prevents out-of-domain layout values. */
    if (value < (long)min_value || value > (long)max_value) {
        write_error_value_message(err_stream, "Config value '", key, "' is outside the allowed range\n");
        return false;
    }

    /* Narrowing cast is validated by the range gate above. */
    *target = (int)value;
    return true;
}

/* Validates and maps textual anchor values to the internal enum. */
static bool parse_anchor_value(const char *json_text, OSDAnchor *target, FILE *err_stream) {
    char value[32];
    bool key_present = osd_config_find_key(json_text, "anchor") != NULL;

    /* Invalid type is accepted only when the key is not present at all. */
    if (!osd_config_parse_string_value(json_text, "anchor", value, sizeof(value))) {
        if (key_present) {
            write_error_text(err_stream, "Config value 'anchor' must be a string\n");
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

    write_error_value_message(err_stream, "Invalid config value for 'anchor': ", value, "\n");
    return false;
}

/* Parses CSS color/gradient fields and rejects empty values. */
static bool parse_color_value(const char *json_text, const char *key, char *target, size_t target_size, FILE *err_stream) {
    bool key_present = osd_config_find_key(json_text, key) != NULL;

    /* Style fields are optional, but malformed provided values are rejected. */
    if (!osd_config_parse_string_value(json_text, key, target, target_size)) {
        if (key_present) {
            write_error_value_message(err_stream, "Config value '", key, "' must be a non-empty string\n");
            return false;
        }
        return true;
    }

    /* Empty values are treated as user input mistakes, not defaults. */
    if (target[0] == '\0') {
        write_error_value_message(err_stream, "Config value '", key, "' cannot be empty\n");
        return false;
    }

    return true;
}

/* Parses optional custom CSS file path; empty string clears custom CSS usage. */
static bool parse_css_file_value(const char *json_text, OSDArgs *args, FILE *err_stream) {
    char value[OSD_CONFIG_PATH_MAX];
    bool key_present = osd_config_find_key(json_text, "css_file") != NULL;
    size_t value_len = 0U;

    /* css_file is optional; validation is strict only when key exists. */
    if (!osd_config_parse_string_value(json_text, "css_file", value, sizeof(value))) {
        if (key_present) {
            write_error_text(err_stream, "Config value 'css_file' must be a string\n");
            return false;
        }
        return true;
    }

    value_len = strlen(value);
    if (value_len == 0U) {
        /* Explicit empty string disables custom CSS loading. */
        args->css_path[0] = '\0';
        args->css_path_set = false;
        return true;
    }

    /* Prevents overflow on the destination path buffer. */
    if (value_len >= sizeof(args->css_path)) {
        write_error_text(err_stream, "Config value 'css_file' is too long\n");
        return false;
    }

    /* Safe copy after strict length check above. */
    if (!copy_cstr_bounded(args->css_path, sizeof(args->css_path), value)) {
        write_error_text(err_stream, "Config value 'css_file' is too long\n");
        return false;
    }
    args->css_path_set = true;
    return true;
}

/* Reads the config file into memory with size bounds and full-read checks. */
static bool osd_config_read_file_text(const char *path, char **out_json_text, FILE *err_stream) {
    FILE *config_file = NULL;
    char *json_text = NULL;
    long file_size = 0L;
    size_t read_size = 0U;

    /* Binary mode preserves exact bytes and avoids newline translation. */
    config_file = fopen(path, "rb");
    if (config_file == NULL) {
        write_error_value_message(err_stream, "Failed to open config file: ", path, "\n");
        return false;
    }

    if (fseek(config_file, 0L, SEEK_END) != 0) {
        write_error_value_message(err_stream, "Failed to seek config file: ", path, "\n");
        (void)fclose(config_file);
        return false;
    }

    file_size = ftell(config_file);
    if (file_size < 0L || file_size > OSD_CONFIG_MAX_SIZE_BYTES) {
        /* Hard size cap blocks memory abuse from oversized config files. */
        write_error_value_message(err_stream, "Invalid config file size for: ", path, "\n");
        (void)fclose(config_file);
        return false;
    }

    if (fseek(config_file, 0L, SEEK_SET) != 0) {
        write_error_value_message(err_stream, "Failed to read config file: ", path, "\n");
        (void)fclose(config_file);
        return false;
    }

    /* +1 stores a guaranteed NUL terminator for parser routines. */
    json_text = calloc((size_t)file_size + 1U, sizeof(char));
    if (json_text == NULL) {
        write_error_text(err_stream, "Failed to allocate memory for config file\n");
        (void)fclose(config_file);
        return false;
    }

    read_size = fread(json_text, 1U, (size_t)file_size, config_file);
    (void)fclose(config_file);

    if (read_size != (size_t)file_size) {
        /* Partial reads are rejected to avoid parsing truncated payloads. */
        write_error_value_message(err_stream, "Failed to fully read config file: ", path, "\n");
        free(json_text);
        return false;
    }

    *out_json_text = json_text;
    return true;
}

/* Applies all numeric values that share CLI-equivalent bounds. */
static bool osd_config_apply_numeric_values(const char *json_text, OSDArgs *args, FILE *err_stream) {
    bool ok = true;

    /* Numeric ranges mirror CLI bounds for consistent UX across inputs. */
    /* Accumulated boolean keeps checking all fields in one parse pass. */
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

/* Parses monitor index separately because it accepts -1 as a sentinel value. */
static bool osd_config_apply_monitor_index(const char *json_text, OSDArgs *args, FILE *err_stream) {
    long monitor_value = 0L;

    /* monitor_index is optional and falls back to existing/default value. */
    if (!osd_config_parse_long_value(json_text, "monitor_index", &monitor_value)) {
        if (osd_config_find_key(json_text, "monitor_index") != NULL) {
            write_error_text(err_stream, "Config value 'monitor_index' must be a valid integer\n");
            return false;
        }
        return true;
    }

    if (monitor_value < -1L || monitor_value > (long)INT_MAX) {
        /* -1 is a documented sentinel for default monitor selection. */
        write_error_text(err_stream, "Config value 'monitor_index' is outside the allowed range\n");
        return false;
    }

    args->monitor_index = (int)monitor_value;
    return true;
}

/* Applies optional boolean toggles for watch and system volume sources. */
static bool osd_config_apply_bool_values(const char *json_text, OSDArgs *args, FILE *err_stream) {
    bool bool_value = false;
    const char *watch_key = osd_config_find_key(json_text, "watch_mode");
    const char *system_key = osd_config_find_key(json_text, "use_system_volume");
    const char *css_replace_key = osd_config_find_key(json_text, "css_replace");
    const char *vertical_key = osd_config_find_key(json_text, "vertical");

    /* Each boolean is parsed only when that specific key exists. */
    if (watch_key != NULL && !osd_config_parse_bool_value(json_text, "watch_mode", &bool_value)) {
        write_error_text(err_stream, "Config value 'watch_mode' must be true or false\n");
        return false;
    }
    if (watch_key != NULL) {
        /* bool_value is intentionally reused to avoid extra temporaries. */
        args->watch_mode = bool_value;
    }
    if (system_key != NULL && !osd_config_parse_bool_value(json_text, "use_system_volume", &bool_value)) {
        write_error_text(err_stream, "Config value 'use_system_volume' must be true or false\n");
        return false;
    }
    if (system_key != NULL) {
        args->use_system_volume = bool_value;
    }
    if (css_replace_key != NULL && !osd_config_parse_bool_value(json_text, "css_replace", &bool_value)) {
        write_error_text(err_stream, "Config value 'css_replace' must be true or false\n");
        return false;
    }
    if (css_replace_key != NULL) {
        args->css_replace = bool_value;
    }
    if (vertical_key != NULL && !osd_config_parse_bool_value(json_text, "vertical", &bool_value)) {
        write_error_text(err_stream, "Config value 'vertical' must be true or false\n");
        return false;
    }
    if (vertical_key != NULL) {
        args->theme.vertical_layout = bool_value;
    }

    return true;
}

/* Applies visual configuration fields independently to allow partial themes. */
static bool osd_config_apply_visual_values(const char *json_text, OSDArgs *args, FILE *err_stream) {
    bool ok = true;

    /* Visual fields remain independent so partial theme updates are valid. */
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

/* Entry point for JSON config loading and structured field application. */
bool osd_config_apply_file(const char *path, OSDArgs *args, FILE *err_stream) {
    char *json_text = NULL;
    OSDArgs parsed_args;
    bool ok = true;
    static const char *const allowed_keys[] = {
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

    if (path == NULL || args == NULL || err_stream == NULL) {
        return false;
    }
    parsed_args = *args;

    /* Fail early when file read fails; helper writes detailed error text. */
    if (!osd_config_read_file_text(path, &json_text, err_stream)) {
        return false;
    }
    /* Structural validation runs before any key/value extraction. */
    if (!osd_config_validate_json_envelope(json_text, err_stream)) {
        free(json_text);
        return false;
    }
    /* Allowlist gate blocks unknown keys and duplicate-key ambiguity. */
    if (!osd_config_validate_top_level_keys(
            json_text,
            allowed_keys,
            sizeof(allowed_keys) / sizeof(allowed_keys[0]),
            err_stream
        )) {
        free(json_text);
        return false;
    }

    ok &= osd_config_apply_numeric_values(json_text, &parsed_args, err_stream);
    ok &= osd_config_apply_monitor_index(json_text, &parsed_args, err_stream);
    ok &= osd_config_apply_bool_values(json_text, &parsed_args, err_stream);
    ok &= osd_config_apply_visual_values(json_text, &parsed_args, err_stream);
    if (ok && parsed_args.css_replace && !parsed_args.css_path_set) {
        /* Replace mode without css_file would produce an unstyled window. */
        write_error_text(err_stream, "Config value 'css_replace' requires a non-empty 'css_file'\n");
        ok = false;
    }

    if (ok) {
        *args = parsed_args;
    }

    free(json_text);
    return ok;
}
