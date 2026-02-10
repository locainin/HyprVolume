#include "parse_internal.h"
#include "common/safeio.h"

#include <limits.h>
#include <string.h>

/* Dispatches command-line options into the resolved runtime argument struct. */
bool osd_args_parse(int argc, char **argv, OSDArgs *out, FILE *err_stream) {
    int index = 0;

    if (out == NULL || argv == NULL || err_stream == NULL) {
        return false;
    }

    OSDIntOption int_options[] = {
        {"--monitor", -1, INT_MAX, &out->monitor_index}
    };
    OSDUIntOption uint_options[] = {
        {"--timeout-ms", 100U, 10000U, &out->timeout_ms},
        {"--watch-poll-ms", 40U, 2000U, &out->watch_poll_ms},
        {"--width", 40U, 1400U, &out->theme.width_px},
        {"--height", 20U, 300U, &out->theme.height_px},
        {"--margin-top", 0U, 500U, &out->theme.margin_y_px},
        {"--radius", 0U, 200U, &out->theme.corner_radius_px},
        {"--icon-size", 8U, 200U, &out->theme.icon_size_px},
        {"--font-size", 8U, 200U, &out->theme.font_size_px}
    };
    OSDColorOption color_options[] = {
        {"--background-color", out->theme.background_color, sizeof(out->theme.background_color)},
        {"--border-color", out->theme.border_color, sizeof(out->theme.border_color)},
        {"--fill-color", out->theme.fill_color, sizeof(out->theme.fill_color)},
        {"--track-color", out->theme.track_color, sizeof(out->theme.track_color)},
        {"--text-color", out->theme.text_color, sizeof(out->theme.text_color)},
        {"--icon-color", out->theme.icon_color, sizeof(out->theme.icon_color)}
    };

    /* Config path is discovered first so a second parse pass can apply overrides. */
    for (index = 1; index < argc; index++) {
        const char *arg = argv[index];
        OSDParseDispatch dispatch = OSD_PARSE_NOT_MATCHED;

        if (strcmp(arg, "--help") == 0) {
            out->show_help = true;
            continue;
        }

        /* Detect --config before other options. */
        dispatch = osd_args_parse_config_option(argc, argv, &index, arg, out, err_stream);
        if (dispatch == OSD_PARSE_ERROR) {
            return false;
        }
        if (dispatch == OSD_PARSE_MATCHED) {
            continue;
        }

        /* Parse simple flag-style options. */
        dispatch = osd_args_parse_toggle_flags(arg, out);
        if (dispatch == OSD_PARSE_MATCHED) {
            continue;
        }

        /* Parse custom CSS path option. */
        dispatch = osd_args_parse_css_file_option(argc, argv, &index, arg, out, err_stream);
        if (dispatch == OSD_PARSE_ERROR) {
            return false;
        }
        if (dispatch == OSD_PARSE_MATCHED) {
            continue;
        }

        /* Parse manual volume setting. */
        dispatch = osd_args_parse_value_option(argc, argv, &index, arg, out, err_stream);
        if (dispatch == OSD_PARSE_ERROR) {
            return false;
        }
        if (dispatch == OSD_PARSE_MATCHED) {
            continue;
        }

        /* Parse bounded signed numeric options. */
        dispatch = osd_args_parse_int_options(
            argc,
            argv,
            &index,
            arg,
            int_options,
            sizeof(int_options) / sizeof(int_options[0]),
            err_stream
        );
        if (dispatch == OSD_PARSE_ERROR) {
            return false;
        }
        if (dispatch == OSD_PARSE_MATCHED) {
            continue;
        }

        /* Parse bounded unsigned numeric options. */
        dispatch = osd_args_parse_uint_options(
            argc,
            argv,
            &index,
            arg,
            uint_options,
            sizeof(uint_options) / sizeof(uint_options[0]),
            err_stream
        );
        if (dispatch == OSD_PARSE_ERROR) {
            return false;
        }
        if (dispatch == OSD_PARSE_MATCHED) {
            continue;
        }

        /* Parse CSS string options. */
        dispatch = osd_args_parse_color_options(
            argc,
            argv,
            &index,
            arg,
            color_options,
            sizeof(color_options) / sizeof(color_options[0]),
            err_stream
        );
        if (dispatch == OSD_PARSE_ERROR) {
            return false;
        }
        if (dispatch == OSD_PARSE_MATCHED) {
            continue;
        }

        (void)osd_io_write_text(err_stream, "Unknown option: ");
        (void)osd_io_write_line(err_stream, arg);
        return false;
    }

    /*
     * Cross-source validation (CLI + config) is deferred until startup has
     * completed config loading, so option parsing can remain order-independent.
     */
    return true;
}
