#ifndef HYPRVOLUME_SYSTEM_VOLUME_PATH_H
#define HYPRVOLUME_SYSTEM_VOLUME_PATH_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    // Path resolution succeeded
    OSD_VOLUME_PATH_ERR_NONE = 0,
    // Caller passed null or invalid output buffers
    OSD_VOLUME_PATH_ERR_INVALID_ARG,
    // Override gate is enabled but override path variable is empty or missing
    OSD_VOLUME_PATH_ERR_OVERRIDE_REQUIRES_PATH,
    // Override path contains newline bytes and is rejected
    OSD_VOLUME_PATH_ERR_OVERRIDE_NEWLINE,
    // Override path is not absolute
    OSD_VOLUME_PATH_ERR_OVERRIDE_NOT_ABSOLUTE,
    // Override path does not reference an executable regular file
    OSD_VOLUME_PATH_ERR_OVERRIDE_NOT_EXECUTABLE,
    // Resolved path cannot fit in caller output buffer
    OSD_VOLUME_PATH_ERR_PATH_TOO_LONG,
    // No trusted default path was found
    OSD_VOLUME_PATH_ERR_NOT_FOUND
} OSDVolumePathError;

typedef enum {
    // Source is not known because resolution failed early
    OSD_VOLUME_PATH_SOURCE_UNKNOWN = 0,
    // Path came from explicit override env variable
    OSD_VOLUME_PATH_SOURCE_OVERRIDE,
    // Path came from validated trusted cache
    OSD_VOLUME_PATH_SOURCE_TRUSTED_CACHE,
    // Path came from trusted default path scan
    OSD_VOLUME_PATH_SOURCE_TRUSTED_DEFAULT
} OSDVolumePathSource;

typedef struct {
    // Resolution status classification
    OSDVolumePathError error;
    // Path source used or attempted
    OSDVolumePathSource source;
    // Optional detail key used by diagnostics
    const char *detail;
} OSDVolumePathStatus;

// Injects getenv for deterministic tests
typedef const char *(*OSDVolumeGetEnvFn)(const char *name);

// Resolves wpctl executable path using trusted defaults and optional gated override
bool osd_volume_resolve_wpctl_path(
    char *out_path,
    size_t out_path_size,
    OSDVolumePathStatus *out_status
);

// Restores default getenv when getenv_fn is null
void osd_volume_path_set_getenv_fn(OSDVolumeGetEnvFn getenv_fn);

#endif
