#include "args/args.h"

#include <string.h>

#define OSD_DEFAULT_VOLUME_PERCENT 50
#define OSD_DEFAULT_TIMEOUT_MS 1400U
#define OSD_DEFAULT_WATCH_POLL_MS 120U
#define OSD_DEFAULT_WIDTH_PX 360U
#define OSD_DEFAULT_HEIGHT_PX 44U
#define OSD_DEFAULT_VERTICAL_LAYOUT false
#define OSD_DEFAULT_MARGIN_X_PX 18U
#define OSD_DEFAULT_MARGIN_Y_PX 20U
#define OSD_DEFAULT_CORNER_RADIUS_PX 10U
#define OSD_DEFAULT_ICON_SIZE_PX 14U
#define OSD_DEFAULT_FONT_SIZE_PX 14U

/* Initializes all runtime arguments to production-safe defaults. */
void osd_args_defaults(OSDArgs *args) {
    if (args == NULL) {
        return;
    }

    args->volume.volume_percent = OSD_DEFAULT_VOLUME_PERCENT;
    args->volume.muted = false;
    args->timeout_ms = OSD_DEFAULT_TIMEOUT_MS;
    args->watch_poll_ms = OSD_DEFAULT_WATCH_POLL_MS;
    args->monitor_index = -1;
    args->config_path[0] = '\0';
    args->config_path_set = false;
    args->css_path[0] = '\0';
    args->css_path_set = false;
    args->css_replace = false;
    args->show_help = false;
    args->watch_mode = false;
    args->use_system_volume = true;

    args->theme.width_px = OSD_DEFAULT_WIDTH_PX;
    args->theme.height_px = OSD_DEFAULT_HEIGHT_PX;
    args->theme.vertical_layout = OSD_DEFAULT_VERTICAL_LAYOUT;
    args->theme.margin_x_px = OSD_DEFAULT_MARGIN_X_PX;
    args->theme.margin_y_px = OSD_DEFAULT_MARGIN_Y_PX;
    args->theme.x_percent = -1;
    args->theme.y_percent = -1;
    args->theme.anchor = OSD_ANCHOR_TOP_CENTER;
    args->theme.corner_radius_px = OSD_DEFAULT_CORNER_RADIUS_PX;
    args->theme.icon_size_px = OSD_DEFAULT_ICON_SIZE_PX;
    args->theme.font_size_px = OSD_DEFAULT_FONT_SIZE_PX;

    /*
     * Default palette:
     * dark glass panel with blue accents and bright text.
     */
    (void)strcpy(args->theme.background_color, "linear-gradient(140deg, rgba(16, 23, 36, 0.95), rgba(20, 30, 46, 0.93))");
    (void)strcpy(args->theme.border_color, "rgba(102, 131, 182, 0.42)");
    (void)strcpy(args->theme.fill_color, "linear-gradient(90deg, #8fd9ff 0%, #72c4ff 52%, #5aa7ff 100%)");
    (void)strcpy(args->theme.track_color, "linear-gradient(90deg, rgba(33, 45, 69, 0.76), rgba(30, 40, 60, 0.74))");
    (void)strcpy(args->theme.text_color, "#e8f2ff");
    (void)strcpy(args->theme.icon_color, "#ffffff");
}
