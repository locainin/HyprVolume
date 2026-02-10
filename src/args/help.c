#include "args/args.h"

/* Prints command usage and all supported runtime options. */
void osd_args_print_help(FILE *out_stream, const char *program_name) {
    const char *name = (program_name == NULL || *program_name == '\0') ? "hyprvolume" : program_name;

    if (out_stream == NULL) {
        return;
    }

    (void)fprintf(
        out_stream,
        "Usage: %s [options]\n"
        "\n"
        "Volume source options:\n"
        "  --from-system          Read current sink volume from wpctl (default).\n"
        "  --no-system            Use manual values from --value/--muted.\n"
        "  --value <0-200>        Manual volume percentage.\n"
        "  --muted                Manual muted state.\n"
        "  --unmuted              Manual unmuted state.\n"
        "  --watch                Poll system volume and show OSD when it changes.\n"
        "  --no-watch             Force single-popup mode even if config enables watch mode.\n"
        "\n"
        "Behavior options:\n"
        "  --timeout-ms <100-10000>  Auto-hide delay in milliseconds (default: 1400).\n"
        "  --watch-poll-ms <40-2000> Poll interval in watch mode (default: 120).\n"
        "  --monitor <index>         Target monitor index (0-based, -1 = default).\n"
        "  --config <path>           Load JSON config file before applying CLI overrides.\n"
        "  --css-file <path>         Load custom GTK CSS file.\n"
        "  --css-replace             Use only custom CSS (skip built-in theme CSS).\n"
        "  --css-append              Keep built-in CSS and append custom CSS overrides.\n"
        "  --vertical                Use vertical OSD layout.\n"
        "  --horizontal              Use horizontal OSD layout.\n"
        "\n"
        "Theme options:\n"
        "  --width <40-1400>\n"
        "  --height <20-300>\n"
        "  --margin-top <0-500>      Vertical offset for top/bottom anchors.\n"
        "  --radius <0-200>\n"
        "  --icon-size <8-200>\n"
        "  --font-size <8-200>\n"
        "  --background-color <css-color>\n"
        "  --border-color <css-color>\n"
        "  --fill-color <css-color>\n"
        "  --track-color <css-color>\n"
        "  --text-color <css-color>\n"
        "  --icon-color <css-color>\n"
        "\n"
        "General:\n"
        "  --help                 Show this help text.\n",
        name
    );
}
