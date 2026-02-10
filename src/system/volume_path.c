#include "system/volume_path.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define OSD_WPCTL_ENV_PATH "HYPRVOLUME_WPCTL_PATH"
#define OSD_WPCTL_ENV_ALLOW_OVERRIDE "HYPRVOLUME_ALLOW_WPCTL_PATH_OVERRIDE"

// Default env accessor used outside tests
static const char *osd_volume_default_getenv(const char *name) {
    return getenv(name);
}

// Swappable env accessor keeps tests deterministic
static OSDVolumeGetEnvFn g_osd_volume_getenv_fn = osd_volume_default_getenv;

// Checks if a candidate executable path points to a regular executable file
static bool is_regular_executable_file(const char *path) {
    struct stat file_stat;

    if (path == NULL || path[0] == '\0') {
        return false;
    }

    // stat follows symlinks and evaluates target file mode
    if (stat(path, &file_stat) != 0) {
        return false;
    }

    // Reject directories, sockets, devices, and other non-regular files
    if (!S_ISREG(file_stat.st_mode)) {
        return false;
    }

    // Final gate checks execute bit for current process credentials
    return access(path, X_OK) == 0;
}

// Override gate must be exactly the literal value "1"
static bool is_env_flag_enabled(const char *value) {
    return value != NULL && strcmp(value, "1") == 0;
}

// Newline bytes are blocked to prevent log and parser ambiguity
static bool contains_newline_chars(const char *value) {
    return value != NULL && (strchr(value, '\n') != NULL || strchr(value, '\r') != NULL);
}

// Status helper centralizes enum and detail updates
static void set_status(OSDVolumePathStatus *status, OSDVolumePathError error, OSDVolumePathSource source, const char *detail) {
    if (status == NULL) {
        return;
    }

    status->error = error;
    status->source = source;
    status->detail = detail;
}

// Bounded copy helper keeps explicit ownership of NUL termination
static void copy_path_text(char *destination, const char *source, size_t source_len) {
    size_t index = 0U;

    for (index = 0U; index < source_len; index++) {
        destination[index] = source[index];
    }
    destination[source_len] = '\0';
}

void osd_volume_path_set_getenv_fn(OSDVolumeGetEnvFn getenv_fn) {
    // Null restores production behavior
    if (getenv_fn == NULL) {
        g_osd_volume_getenv_fn = osd_volume_default_getenv;
        return;
    }

    g_osd_volume_getenv_fn = getenv_fn;
}

// Resolves wpctl with fixed path rules and no PATH search
bool osd_volume_resolve_wpctl_path(
    char *out_path,
    size_t out_path_size,
    OSDVolumePathStatus *out_status
) {
    // Trusted cache is used only for default path discovery branch
    static bool has_cached_path = false;
    static char cached_path[256];
    // Default search list is deterministic and avoids PATH lookup
    static const char *const default_paths[] = {
        "/usr/bin/wpctl",
        "/usr/local/bin/wpctl",
        "/bin/wpctl"
    };
    const char *override_path = NULL;
    const char *allow_override = NULL;
    size_t index = 0U;

    if (out_path == NULL || out_path_size == 0U || out_status == NULL) {
        set_status(out_status, OSD_VOLUME_PATH_ERR_INVALID_ARG, OSD_VOLUME_PATH_SOURCE_UNKNOWN, NULL);
        return false;
    }

    set_status(out_status, OSD_VOLUME_PATH_ERR_NONE, OSD_VOLUME_PATH_SOURCE_UNKNOWN, NULL);

    // Read override controls through the injectable accessor
    override_path = g_osd_volume_getenv_fn(OSD_WPCTL_ENV_PATH);
    allow_override = g_osd_volume_getenv_fn(OSD_WPCTL_ENV_ALLOW_OVERRIDE);

    // Override branch is explicit opt-in to reduce accidental unsafe path use
    if (is_env_flag_enabled(allow_override)) {
        if (override_path == NULL || override_path[0] == '\0') {
            set_status(
                out_status,
                OSD_VOLUME_PATH_ERR_OVERRIDE_REQUIRES_PATH,
                OSD_VOLUME_PATH_SOURCE_OVERRIDE,
                OSD_WPCTL_ENV_PATH
            );
            return false;
        }
        if (contains_newline_chars(override_path)) {
            set_status(
                out_status,
                OSD_VOLUME_PATH_ERR_OVERRIDE_NEWLINE,
                OSD_VOLUME_PATH_SOURCE_OVERRIDE,
                OSD_WPCTL_ENV_PATH
            );
            return false;
        }
        if (override_path[0] != '/') {
            set_status(
                out_status,
                OSD_VOLUME_PATH_ERR_OVERRIDE_NOT_ABSOLUTE,
                OSD_VOLUME_PATH_SOURCE_OVERRIDE,
                OSD_WPCTL_ENV_PATH
            );
            return false;
        }
        if (!is_regular_executable_file(override_path)) {
            set_status(
                out_status,
                OSD_VOLUME_PATH_ERR_OVERRIDE_NOT_EXECUTABLE,
                OSD_VOLUME_PATH_SOURCE_OVERRIDE,
                OSD_WPCTL_ENV_PATH
            );
            return false;
        }
        if (strlen(override_path) >= out_path_size) {
            set_status(
                out_status,
                OSD_VOLUME_PATH_ERR_PATH_TOO_LONG,
                OSD_VOLUME_PATH_SOURCE_OVERRIDE,
                OSD_WPCTL_ENV_PATH
            );
            return false;
        }

        // Override path is never cached and is revalidated each call
        copy_path_text(out_path, override_path, strlen(override_path));
        set_status(out_status, OSD_VOLUME_PATH_ERR_NONE, OSD_VOLUME_PATH_SOURCE_OVERRIDE, NULL);
        return true;
    }

    // Cached trusted path is revalidated before reuse
    if (has_cached_path) {
        if (!is_regular_executable_file(cached_path)) {
            // Drop cache when executable disappears or stops being executable
            has_cached_path = false;
            cached_path[0] = '\0';
        } else {
            size_t cached_size = strlen(cached_path);

            if (cached_size >= out_path_size) {
                set_status(
                    out_status,
                    OSD_VOLUME_PATH_ERR_PATH_TOO_LONG,
                    OSD_VOLUME_PATH_SOURCE_TRUSTED_CACHE,
                    NULL
                );
                return false;
            }

            copy_path_text(out_path, cached_path, cached_size);
            set_status(out_status, OSD_VOLUME_PATH_ERR_NONE, OSD_VOLUME_PATH_SOURCE_TRUSTED_CACHE, NULL);
            return true;
        }
    }

    // Trusted fallback scan runs in fixed order for stable behavior
    for (index = 0U; index < (sizeof(default_paths) / sizeof(default_paths[0])); index++) {
        if (!is_regular_executable_file(default_paths[index])) {
            continue;
        }
        if (strlen(default_paths[index]) >= out_path_size) {
            continue;
        }

        copy_path_text(out_path, default_paths[index], strlen(default_paths[index]));
        if (strlen(default_paths[index]) < sizeof(cached_path)) {
            // Cache only trusted defaults that fit internal cache buffer
            copy_path_text(cached_path, default_paths[index], strlen(default_paths[index]));
            has_cached_path = true;
        }
        set_status(out_status, OSD_VOLUME_PATH_ERR_NONE, OSD_VOLUME_PATH_SOURCE_TRUSTED_DEFAULT, NULL);
        return true;
    }

    set_status(out_status, OSD_VOLUME_PATH_ERR_NOT_FOUND, OSD_VOLUME_PATH_SOURCE_UNKNOWN, NULL);
    return false;
}
