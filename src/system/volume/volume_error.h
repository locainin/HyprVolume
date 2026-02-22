#ifndef HYPRVOLUME_SYSTEM_VOLUME_ERROR_H
#define HYPRVOLUME_SYSTEM_VOLUME_ERROR_H

#include "system/volume/volume_parse.h"
#include "system/volume/volume_path.h"
#include "system/volume/volume_proc.h"

#include <stdio.h>

// Writes canonical resolve-phase diagnostics
void osd_volume_write_resolve_error(FILE *err_stream, const OSDVolumePathStatus *status);

// Writes canonical process-phase diagnostics
void osd_volume_write_proc_error(
    FILE *err_stream,
    const char *wpctl_path,
    OSDVolumePathSource source,
    const OSDVolumeProcStatus *status
);

// Writes canonical parse-phase diagnostics with sanitized line preview
void osd_volume_write_parse_error(
    FILE *err_stream,
    const char *wpctl_path,
    OSDVolumePathSource source,
    const char *line,
    const OSDVolumeParseStatus *status
);

#endif
