#include "args/args.h"
#include "config/config.h"
#include "window/window.h"

#include <stdio.h>
#include <stdlib.h>

/* Application entrypoint: defaults -> parse -> optional config -> window runtime. */
int main(int argc, char **argv) {
    OSDArgs args;

    osd_args_defaults(&args);

    if (!osd_args_parse(argc, argv, &args, stderr)) {
        osd_args_print_help(stderr, (argc > 0) ? argv[0] : "hyprvolume");
        return EXIT_FAILURE;
    }

    if (args.show_help) {
        osd_args_print_help(stdout, (argc > 0) ? argv[0] : "hyprvolume");
        return EXIT_SUCCESS;
    }

    if (args.config_path_set) {
        if (!osd_config_apply_file(args.config_path, &args, stderr)) {
            return EXIT_FAILURE;
        }

        if (!osd_args_parse(argc, argv, &args, stderr)) {
            osd_args_print_help(stderr, (argc > 0) ? argv[0] : "hyprvolume");
            return EXIT_FAILURE;
        }

        if (args.show_help) {
            osd_args_print_help(stdout, (argc > 0) ? argv[0] : "hyprvolume");
            return EXIT_SUCCESS;
        }
    }

    return osd_window_run(&args);
}
