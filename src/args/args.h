#ifndef HYPRVOLUME_ARGS_H
#define HYPRVOLUME_ARGS_H

#include <stdbool.h>
#include <stdio.h>

/* Upper bound for --config path and config_path JSON field. */
#define OSD_CONFIG_PATH_MAX 4096U

/* Runtime volume sample shown in the popup. */
typedef struct {
    int volume_percent;
    bool muted;
} OSDVolumeState;

/* Anchor fallback used when percent placement is not configured. */
typedef enum {
    OSD_ANCHOR_TOP_CENTER = 0,
    OSD_ANCHOR_TOP_LEFT = 1,
    OSD_ANCHOR_TOP_RIGHT = 2,
    OSD_ANCHOR_BOTTOM_CENTER = 3,
    OSD_ANCHOR_BOTTOM_LEFT = 4,
    OSD_ANCHOR_BOTTOM_RIGHT = 5
} OSDAnchor;

/* Theme values are interpreted as pixels or CSS values unless noted. */
typedef struct {
    unsigned int width_px;
    unsigned int height_px;
    bool vertical_layout;
    unsigned int margin_x_px;
    unsigned int margin_y_px;
    int x_percent;
    int y_percent;
    OSDAnchor anchor;
    unsigned int corner_radius_px;
    unsigned int icon_size_px;
    unsigned int font_size_px;
    char background_color[128];
    char border_color[96];
    char fill_color[128];
    char track_color[96];
    char text_color[96];
    char icon_color[96];
} OSDTheme;

/* Final resolved settings after defaults, config file, and CLI override pass. */
typedef struct {
    OSDVolumeState volume;
    unsigned int timeout_ms;
    unsigned int watch_poll_ms;
    int monitor_index;
    char config_path[OSD_CONFIG_PATH_MAX];
    bool config_path_set;
    char css_path[OSD_CONFIG_PATH_MAX];
    bool css_path_set;
    bool css_replace;
    bool show_help;
    bool watch_mode;
    bool use_system_volume;
    OSDTheme theme;
} OSDArgs;

void osd_args_defaults(OSDArgs *args);

// Parses CLI flags into out
// Returns false when input is invalid
bool osd_args_parse(int argc, char **argv, OSDArgs *out, FILE *err_stream);

// Prints CLI help text
void osd_args_print_help(FILE *out_stream, const char *program_name);

#endif
