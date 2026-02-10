#include "config/config.h"

#include "config/apply.h"
#include "config/error.h"
#include "config/io.h"
#include "config/json/json_config_fields.h"
#include "config/schema.h"

#include <stdlib.h>

// Entry point for JSON config loading and structured field application
bool osd_config_apply_file(const char *path, OSDArgs *args, FILE *err_stream) {
    char *json_text = NULL;
    OSDArgs parsed_args;
    bool ok = true;

    if (path == NULL || args == NULL || err_stream == NULL) {
        return false;
    }
    parsed_args = *args;

    if (!osd_config_read_file_text(path, &json_text, err_stream)) {
        return false;
    }

    if (!osd_config_validate_json_envelope(json_text, err_stream)) {
        free(json_text);
        return false;
    }

    if (!osd_config_schema_validate_top_level_keys(json_text, err_stream)) {
        free(json_text);
        return false;
    }

    ok &= osd_config_apply_numeric_values(json_text, &parsed_args, err_stream);
    ok &= osd_config_apply_monitor_index(json_text, &parsed_args, err_stream);
    ok &= osd_config_apply_bool_values(json_text, &parsed_args, err_stream);
    ok &= osd_config_apply_visual_values(json_text, &parsed_args, err_stream);

    if (ok && parsed_args.css_replace && !parsed_args.css_path_set) {
        osd_config_write_error_text(err_stream, "Config value 'css_replace' requires a non-empty 'css_file'\n");
        ok = false;
    }

    if (ok) {
        *args = parsed_args;
    }

    free(json_text);
    return ok;
}
