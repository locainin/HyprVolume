#include "system/volume.h"
#include "system/volume/volume_error.h"
#include "system/volume/volume_path.h"

#define OSD_VOLUME_WPCTL_PATH_MAX 256U
#define OSD_VOLUME_WPCTL_LINE_MAX 256U

bool osd_system_volume_query(OSDVolumeState *out_state, FILE *err_stream) {
    OSDVolumePathStatus path_status;
    OSDVolumeProcStatus proc_status;
    OSDVolumeParseStatus parse_status;
    OSDVolumeState parsed_state;
    char wpctl_path[OSD_VOLUME_WPCTL_PATH_MAX];
    char line[OSD_VOLUME_WPCTL_LINE_MAX];

    if (out_state == NULL || err_stream == NULL) {
        return false;
    }

    // Step 1 resolve executable path with override and trusted-path policy
    if (!osd_volume_resolve_wpctl_path(wpctl_path, sizeof(wpctl_path), &path_status)) {
        osd_volume_write_resolve_error(err_stream, &path_status);
        return false;
    }

    // Step 2 run wpctl and capture one stdout line with timeout protection
    if (!osd_volume_run_wpctl_get_volume_line(wpctl_path, line, sizeof(line), &proc_status)) {
        osd_volume_write_proc_error(err_stream, wpctl_path, path_status.source, &proc_status);
        return false;
    }

    // Step 3 parse and normalize output into final state struct
    if (!osd_volume_parse_wpctl_line(line, &parsed_state, &parse_status)) {
        osd_volume_write_parse_error(err_stream, wpctl_path, path_status.source, line, &parse_status);
        return false;
    }

    // Commit parsed result only on full success
    *out_state = parsed_state;
    return true;
}
