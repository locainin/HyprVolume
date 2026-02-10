#include "system/volume.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <spawn.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define OSD_WPCTL_ENV_PATH "HYPRVOLUME_WPCTL_PATH"
#define OSD_WPCTL_ENV_ALLOW_OVERRIDE "HYPRVOLUME_ALLOW_WPCTL_PATH_OVERRIDE"

extern char **environ;

/* Checks if a candidate executable path points to a regular executable file. */
static bool is_regular_executable_file(const char *path) {
    struct stat file_stat;

    /* Empty paths are rejected early to avoid ambiguous resolution. */
    if (path == NULL || path[0] == '\0') {
        return false;
    }

    // stat follows symlinks and validates the target object type
    if (stat(path, &file_stat) != 0) {
        return false;
    }

    /* Restrict execution targets to regular files only. */
    if (!S_ISREG(file_stat.st_mode)) {
        return false;
    }

    /* Final check verifies execute bit for current process credentials. */
    return access(path, X_OK) == 0;
}

static bool is_env_flag_enabled(const char *value) {
    return value != NULL && strcmp(value, "1") == 0;
}

static bool contains_newline_chars(const char *value) {
    return value != NULL && (strchr(value, '\n') != NULL || strchr(value, '\r') != NULL);
}

// Resolves wpctl with fixed path rules and no PATH search
// Optional override is used only when the explicit env gate is enabled
static bool resolve_wpctl_path(char *out_path, size_t out_path_size, FILE *err_stream) {
    static bool has_cached_path = false;
    static char cached_path[256];
    static const char *const default_paths[] = {
        "/usr/bin/wpctl",
        "/usr/local/bin/wpctl",
        "/bin/wpctl"
    };
    const char *override_path = NULL;
    const char *allow_override = NULL;
    size_t index = 0U;

    if (out_path == NULL || out_path_size == 0U || err_stream == NULL) {
        return false;
    }

    override_path = getenv(OSD_WPCTL_ENV_PATH);
    allow_override = getenv(OSD_WPCTL_ENV_ALLOW_OVERRIDE);
    // Env path is used only when the gate is set to exact value 1
    if (is_env_flag_enabled(allow_override)) {
        if (override_path == NULL || override_path[0] == '\0') {
            (void)fprintf(
                err_stream,
                "%s=1 requires %s to be set to a non-empty absolute path\n",
                OSD_WPCTL_ENV_ALLOW_OVERRIDE,
                OSD_WPCTL_ENV_PATH
            );
            return false;
        }
        if (contains_newline_chars(override_path)) {
            (void)fprintf(err_stream, "%s must not contain newline characters\n", OSD_WPCTL_ENV_PATH);
            return false;
        }
        /* Override path must be absolute to prevent PATH-based hijacks. */
        if (override_path[0] != '/') {
            (void)fprintf(err_stream, "%s must be an absolute path when set\n", OSD_WPCTL_ENV_PATH);
            return false;
        }
        if (!is_regular_executable_file(override_path)) {
            (void)fprintf(err_stream, "%s does not point to an executable file: %s\n", OSD_WPCTL_ENV_PATH, override_path);
            return false;
        }
        if (strlen(override_path) >= out_path_size) {
            (void)fprintf(err_stream, "Resolved wpctl path is too long\n");
            return false;
        }
        /* Copy is safe because output buffer size was checked. */
        (void)strcpy(out_path, override_path);
        return true;
    }

    // Cache is used only for trusted fixed paths
    if (has_cached_path) {
        if (!is_regular_executable_file(cached_path)) {
            // Cache is cleared when executable disappears or becomes invalid
            has_cached_path = false;
            cached_path[0] = '\0';
        } else {
            size_t cached_size = strlen(cached_path);

            if (cached_size >= out_path_size) {
                (void)fprintf(err_stream, "Resolved wpctl path is too long\n");
                return false;
            }
            // Fast path avoids repeated file checks during watch polling
            (void)strcpy(out_path, cached_path);
            return true;
        }
    }

    for (index = 0U; index < (sizeof(default_paths) / sizeof(default_paths[0])); index++) {
        // Check fixed trusted paths in stable order
        /* Iterate deterministic trusted paths in fixed priority order. */
        if (!is_regular_executable_file(default_paths[index])) {
            continue;
        }
        if (strlen(default_paths[index]) >= out_path_size) {
            continue;
        }
        /* Copy is safe because destination bound was validated. */
        (void)strcpy(out_path, default_paths[index]);
        if (strlen(default_paths[index]) < sizeof(cached_path)) {
            (void)strcpy(cached_path, default_paths[index]);
            has_cached_path = true;
        }
        return true;
    }

    (void)fprintf(
        err_stream,
        "Failed to locate wpctl in standard paths; set %s to an absolute executable path and %s=1 to opt in\n",
        OSD_WPCTL_ENV_PATH,
        OSD_WPCTL_ENV_ALLOW_OVERRIDE
    );
    return false;
}

/* Clamp utility keeps parsed output inside the visible OSD range. */
static int clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) {
        /* Clamp avoids sending invalid ranges to progress UI. */
        return min_value;
    }

    if (value > max_value) {
        /* Clamp avoids sending invalid ranges to progress UI. */
        return max_value;
    }

    return value;
}

static const char *skip_whitespace(const char *text) {
    while (text != NULL && *text != '\0' && isspace((unsigned char)*text) != 0) {
        /* Advancing one byte at a time keeps parser branchless/simple. */
        text++;
    }
    return text;
}

/* Waits for a child process while tolerating EINTR interruptions. */
static bool wait_for_child_process(pid_t child_pid, int *status, FILE *err_stream) {
    pid_t wait_result = (pid_t)0;

    if (status == NULL || err_stream == NULL) {
        return false;
    }

    do {
        /* EINTR retry prevents transient signal interruptions from failing. */
        wait_result = waitpid(child_pid, status, 0);
    } while (wait_result < 0 && errno == EINTR);

    if (wait_result < 0) {
        (void)fprintf(err_stream, "Failed to wait for wpctl process\n");
        return false;
    }

    return true;
}

/*
 * Parses wpctl output while tolerating known output variants:
 * - "Volume: <fraction> [MUTED]"
 * - "Volume for <target>: <percent>% [MUTED]"
 */
static bool parse_wpctl_output_line(const char *line, double *out_fraction, bool *out_muted, FILE *err_stream) {
    const char *cursor = NULL;
    const char *prefix_end = NULL;
    char *end_ptr = NULL;
    double parsed_value = 0.0;
    double parsed_fraction = 0.0;
    bool parsed_as_percent = false;
    bool muted = false;

    if (line == NULL || out_fraction == NULL || out_muted == NULL || err_stream == NULL) {
        return false;
    }

    cursor = skip_whitespace(line);
    if (cursor == NULL) {
        return false;
    }

    /*
     * Prefix matching is explicit to keep parser behavior deterministic across
     * WirePlumber output variants.
     */
    if (strncmp(cursor, "Volume:", 7) == 0) {
        prefix_end = cursor + 7;
    } else if (strncmp(cursor, "Volume for ", 11) == 0) {
        prefix_end = strchr(cursor, ':');
        if (prefix_end != NULL) {
            prefix_end++;
        }
    }

    if (prefix_end == NULL) {
        (void)fprintf(err_stream, "Unexpected wpctl output: %s", line);
        return false;
    }

    cursor = skip_whitespace(prefix_end);
    if (cursor == NULL || *cursor == '\0') {
        (void)fprintf(err_stream, "Unexpected wpctl output: %s", line);
        return false;
    }

    errno = 0;
    /*
     * strtod accepts both integer and fractional values.
     * Unit is resolved by optional '%' suffix handling below.
     */
    parsed_value = strtod(cursor, &end_ptr);
    if (end_ptr == cursor || errno != 0 || !isfinite(parsed_value) || parsed_value < 0.0) {
        (void)fprintf(err_stream, "Invalid wpctl volume value: %s", line);
        return false;
    }

    cursor = skip_whitespace(end_ptr);
    if (cursor != NULL && *cursor == '%') {
        /* Percent output is normalized into the same fraction scale as 0.65. */
        parsed_as_percent = true;
        cursor = skip_whitespace(cursor + 1);
    }

    if (cursor != NULL && strncmp(cursor, "[MUTED]", 7) == 0) {
        /* Optional [MUTED] marker is consumed when present. */
        muted = true;
        cursor = skip_whitespace(cursor + 7);
    }

    if (cursor != NULL && *cursor != '\0') {
        (void)fprintf(err_stream, "Unexpected wpctl output tail: %s", line);
        return false;
    }

    parsed_fraction = parsed_as_percent ? (parsed_value / 100.0) : parsed_value;
    *out_fraction = parsed_fraction;
    *out_muted = muted;
    return true;
}

/* Executes wpctl directly without a shell and captures a single stdout line. */
static bool read_wpctl_volume_line(char *line, size_t line_size, FILE *err_stream) {
    int pipe_fds[2] = {-1, -1};
    pid_t child_pid = -1;
    char wpctl_path[256];
    size_t line_used = 0U;
    int status = 0;
    bool line_truncated = false;
    posix_spawn_file_actions_t spawn_actions;
    int spawn_result = 0;
    char *const wpctl_argv[] = {"wpctl", "get-volume", "@DEFAULT_AUDIO_SINK@", NULL};

    if (line == NULL || line_size == 0U || err_stream == NULL) {
        return false;
    }

    // sizeof(wpctl_path) is used so buffer and size stay linked
    if (!resolve_wpctl_path(wpctl_path, sizeof(wpctl_path), err_stream)) {
        /* Path resolution errors are already emitted by helper. */
        return false;
    }

    if (pipe(pipe_fds) != 0) {
        (void)fprintf(err_stream, "Failed to create pipe for wpctl invocation\n");
        return false;
    }

    if (posix_spawn_file_actions_init(&spawn_actions) != 0) {
        (void)fprintf(err_stream, "Failed to initialize spawn actions for wpctl invocation\n");
        (void)close(pipe_fds[0]);
        (void)close(pipe_fds[1]);
        return false;
    }

    if (posix_spawn_file_actions_addclose(&spawn_actions, pipe_fds[0]) != 0 ||
        posix_spawn_file_actions_adddup2(&spawn_actions, pipe_fds[1], STDOUT_FILENO) != 0 ||
        posix_spawn_file_actions_addclose(&spawn_actions, pipe_fds[1]) != 0) {
        /*
         * Child stderr intentionally inherits the parent stream so operational
         * failures from wpctl stay visible during long-running watch mode.
         */
        (void)fprintf(err_stream, "Failed to prepare wpctl spawn redirections\n");
        (void)posix_spawn_file_actions_destroy(&spawn_actions);
        (void)close(pipe_fds[0]);
        (void)close(pipe_fds[1]);
        return false;
    }

    /* Spawn avoids a full fork and lowers overhead in watch polling loops. */
    spawn_result = posix_spawn(&child_pid, wpctl_path, &spawn_actions, NULL, wpctl_argv, environ);
    (void)posix_spawn_file_actions_destroy(&spawn_actions);
    if (spawn_result != 0) {
        (void)fprintf(err_stream, "Failed to spawn wpctl process: %s\n", strerror(spawn_result));
        (void)close(pipe_fds[0]);
        (void)close(pipe_fds[1]);
        return false;
    }

    (void)close(pipe_fds[1]);
    line[0] = '\0';
    // Read only the first line from wpctl output
    while (line_used < (line_size - 1U)) {
        ssize_t bytes_read = read(pipe_fds[0], line + line_used, line_size - 1U - line_used);
        size_t offset = 0U;

        if (bytes_read < 0) {
            if (errno == EINTR) {
                /* Retry interrupted reads without dropping stream state. */
                continue;
            }
            (void)close(pipe_fds[0]);
            (void)wait_for_child_process(child_pid, &status, err_stream);
            (void)fprintf(err_stream, "Failed to read volume from wpctl output\n");
            return false;
        }

        if (bytes_read == 0) {
            /* EOF: child closed stdout. */
            break;
        }

        while (offset < (size_t)bytes_read) {
            if (line[line_used + offset] == '\n') {
                // One line is enough for current parser format
                /* First line is sufficient for wpctl get-volume output. */
                line_used += offset + 1U;
                goto read_complete;
            }
            offset++;
        }

        line_used += (size_t)bytes_read;
    }
    if (line_used == (line_size - 1U)) {
        line_truncated = true;
    }

read_complete:
    // Keep writes inside the line buffer
    if (line_used >= line_size) {
        line_used = line_size - 1U;
        line_truncated = true;
    }
    /* Ensure parser always receives a terminated C string. */
    line[line_used] = '\0';
    (void)close(pipe_fds[0]);
    if (line_truncated || line_used == 0U) {
        if (!wait_for_child_process(child_pid, &status, err_stream)) {
            return false;
        }
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            (void)fprintf(err_stream, "wpctl get-volume failed with exit code %d\n", WEXITSTATUS(status));
            return false;
        }
        if (!WIFEXITED(status)) {
            (void)fprintf(err_stream, "wpctl get-volume terminated unexpectedly\n");
            return false;
        }
        if (line_truncated) {
            (void)fprintf(err_stream, "wpctl output exceeds internal line buffer\n");
            return false;
        }
        (void)fprintf(err_stream, "wpctl get-volume produced no stdout output\n");
        return false;
    }

    if (!wait_for_child_process(child_pid, &status, err_stream)) {
        /* wait helper already emitted the failure message. */
        return false;
    }

    if (!WIFEXITED(status)) {
        (void)fprintf(err_stream, "wpctl get-volume terminated unexpectedly\n");
        return false;
    }

    if (WEXITSTATUS(status) != 0) {
        (void)fprintf(err_stream, "wpctl get-volume failed with exit code %d\n", WEXITSTATUS(status));
        return false;
    }

    return true;
}

/* Queries PipeWire volume from wpctl and maps it into OSD-visible state. */
bool osd_system_volume_query(OSDVolumeState *out_state, FILE *err_stream) {
    char line[256];
    double fraction = 0.0;
    bool muted = false;

    if (out_state == NULL || err_stream == NULL) {
        return false;
    }

    if (!read_wpctl_volume_line(line, sizeof(line), err_stream)) {
        /* Error details are emitted by read helper. */
        return false;
    }

    if (!parse_wpctl_output_line(line, &fraction, &muted, err_stream)) {
        /* Parser helper reports malformed output details. */
        return false;
    }

    /* wpctl fractional output maps directly to 0..200 UI percentage scale. */
    out_state->volume_percent = clamp_int((int)lround(fraction * 100.0), 0, 200);
    out_state->muted = muted;
    return true;
}
