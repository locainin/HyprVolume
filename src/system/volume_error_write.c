#include "system/volume_error.h"
#include "system/volume_error_internal.h"
#include <string.h>

// Resolve-phase errors use their own prefix so failures before process spawn are clear
void osd_volume_write_resolve_error(FILE *err_stream, const OSDVolumePathStatus *status) {
    OSDVolumeErrorLine line;

    // Guard invalid call sites and avoid null dereference on status fields
    if (err_stream == NULL || status == NULL) {
        return;
    }

    // Each call builds a fresh line so partial state never leaks across calls
    osd_volume_error_line_init(&line);

    // Each case emits exactly one canonical line
    switch (status->error) {
    case OSD_VOLUME_PATH_ERR_INVALID_ARG:
        osd_volume_error_line_append_text(&line, "volume resolve failed: invalid arguments\n");
        break;
    case OSD_VOLUME_PATH_ERR_OVERRIDE_REQUIRES_PATH:
        // Override gate enabled but path var is missing or empty
        osd_volume_error_line_append_text(&line, "volume resolve failed: override source=");
        osd_volume_error_line_append_text(&line, osd_volume_error_path_source_label(status->source));
        osd_volume_error_line_append_text(&line, " requires env path key=");
        osd_volume_error_line_append_text(&line, osd_volume_error_safe_detail_text(status->detail));
        osd_volume_error_line_append_text(&line, "\n");
        break;
    case OSD_VOLUME_PATH_ERR_OVERRIDE_NEWLINE:
        // Newline in env value is blocked to keep logs and parsing stable
        osd_volume_error_line_append_text(&line, "volume resolve failed: override source=");
        osd_volume_error_line_append_text(&line, osd_volume_error_path_source_label(status->source));
        osd_volume_error_line_append_text(&line, " env key=");
        osd_volume_error_line_append_text(&line, osd_volume_error_safe_detail_text(status->detail));
        osd_volume_error_line_append_text(&line, " contains newline\n");
        break;
    case OSD_VOLUME_PATH_ERR_OVERRIDE_NOT_ABSOLUTE:
        // Override must be absolute to avoid PATH resolution ambiguity
        osd_volume_error_line_append_text(&line, "volume resolve failed: override source=");
        osd_volume_error_line_append_text(&line, osd_volume_error_path_source_label(status->source));
        osd_volume_error_line_append_text(&line, " env key=");
        osd_volume_error_line_append_text(&line, osd_volume_error_safe_detail_text(status->detail));
        osd_volume_error_line_append_text(&line, " must be absolute\n");
        break;
    case OSD_VOLUME_PATH_ERR_OVERRIDE_NOT_EXECUTABLE:
        // Regular executable file is required before process stage
        osd_volume_error_line_append_text(&line, "volume resolve failed: override source=");
        osd_volume_error_line_append_text(&line, osd_volume_error_path_source_label(status->source));
        osd_volume_error_line_append_text(&line, " env key=");
        osd_volume_error_line_append_text(&line, osd_volume_error_safe_detail_text(status->detail));
        osd_volume_error_line_append_text(&line, " not executable regular file\n");
        break;
    case OSD_VOLUME_PATH_ERR_PATH_TOO_LONG:
        // Resolved path did not fit destination buffer
        osd_volume_error_line_append_text(&line, "volume resolve failed: source=");
        osd_volume_error_line_append_text(&line, osd_volume_error_path_source_label(status->source));
        osd_volume_error_line_append_text(&line, " resolved path too long\n");
        break;
    case OSD_VOLUME_PATH_ERR_NOT_FOUND:
        // Trusted path scan failed and override remains opt-in
        osd_volume_error_line_append_text(&line, "volume resolve failed: no trusted wpctl path found, set HYPRVOLUME_ALLOW_WPCTL_PATH_OVERRIDE=1 and HYPRVOLUME_WPCTL_PATH\n");
        break;
    case OSD_VOLUME_PATH_ERR_NONE:
    default:
        // Defensive fallback for enum drift or unexpected status values
        osd_volume_error_line_append_text(&line, "volume resolve failed: unknown error\n");
        break;
    }

    // Emitted once per failure to avoid duplicate resolve diagnostics
    osd_volume_error_line_emit(err_stream, &line);
}

// Process-phase errors include phase/source/path discriminators plus optional numeric detail
void osd_volume_write_proc_error(
    FILE *err_stream,
    const char *wpctl_path,
    OSDVolumePathSource source,
    const OSDVolumeProcStatus *status
) {
    OSDVolumeErrorLine line;

    // Callers pass path/source from resolved phase and status from proc phase
    if (err_stream == NULL || wpctl_path == NULL || status == NULL) {
        return;
    }

    // Fresh builder per call keeps output deterministic
    osd_volume_error_line_init(&line);

    // Error kind maps directly to canonical text used by regression checks
    switch (status->error) {
    case OSD_VOLUME_PROC_ERR_INVALID_ARG:
        osd_volume_error_line_append_query_prefix(&line, "spawn", source, wpctl_path);
        osd_volume_error_line_append_text(&line, "invalid arguments\n");
        break;
    case OSD_VOLUME_PROC_ERR_PIPE:
        osd_volume_error_line_append_query_prefix(&line, "spawn", source, wpctl_path);
        osd_volume_error_line_append_text(&line, "pipe setup failed\n");
        break;
    case OSD_VOLUME_PROC_ERR_SPAWN_ACTIONS_INIT:
    case OSD_VOLUME_PROC_ERR_SPAWN_ACTIONS_PREP:
        // Two setup errors intentionally share one canonical message
        osd_volume_error_line_append_query_prefix(&line, "spawn", source, wpctl_path);
        osd_volume_error_line_append_text(&line, "spawn action setup failed\n");
        break;
    case OSD_VOLUME_PROC_ERR_SPAWN:
        // Include spawn errno code and text for root-cause clarity
        osd_volume_error_line_append_query_prefix(&line, "spawn", source, wpctl_path);
        osd_volume_error_line_append_text(&line, "spawn failed code=");
        osd_volume_error_line_append_int(&line, status->spawn_error);
        osd_volume_error_line_append_text(&line, " (");
        osd_volume_error_line_append_text(&line, strerror(status->spawn_error));
        osd_volume_error_line_append_text(&line, ")\n");
        break;
    case OSD_VOLUME_PROC_ERR_POLL:
        osd_volume_error_line_append_query_prefix(&line, "read", source, wpctl_path);
        osd_volume_error_line_append_text(&line, "poll failed\n");
        break;
    case OSD_VOLUME_PROC_ERR_READ_TIMEOUT:
        // Timeout is reported under its own phase to avoid read error layering
        osd_volume_error_line_append_query_prefix(&line, "timeout", source, wpctl_path);
        osd_volume_error_line_append_text(&line, "wpctl get-volume timed out\n");
        break;
    case OSD_VOLUME_PROC_ERR_POLL_STATE:
        osd_volume_error_line_append_query_prefix(&line, "read", source, wpctl_path);
        osd_volume_error_line_append_text(&line, "invalid poll state\n");
        break;
    case OSD_VOLUME_PROC_ERR_READ:
        osd_volume_error_line_append_query_prefix(&line, "read", source, wpctl_path);
        osd_volume_error_line_append_text(&line, "read failed\n");
        break;
    case OSD_VOLUME_PROC_ERR_OUTPUT_TRUNCATED:
        osd_volume_error_line_append_query_prefix(&line, "read", source, wpctl_path);
        osd_volume_error_line_append_text(&line, "output truncated\n");
        break;
    case OSD_VOLUME_PROC_ERR_OUTPUT_EMPTY:
        osd_volume_error_line_append_query_prefix(&line, "read", source, wpctl_path);
        osd_volume_error_line_append_text(&line, "empty output\n");
        break;
    case OSD_VOLUME_PROC_ERR_WAIT:
        osd_volume_error_line_append_query_prefix(&line, "wait", source, wpctl_path);
        osd_volume_error_line_append_text(&line, "wait failed\n");
        break;
    case OSD_VOLUME_PROC_ERR_EXIT_UNEXPECTED:
        osd_volume_error_line_append_query_prefix(&line, "wait", source, wpctl_path);
        osd_volume_error_line_append_text(&line, "unexpected child exit state\n");
        break;
    case OSD_VOLUME_PROC_ERR_EXIT_SIGNALED:
        // Preserve terminating signal for operator triage
        osd_volume_error_line_append_query_prefix(&line, "wait", source, wpctl_path);
        osd_volume_error_line_append_text(&line, "child signaled signal=");
        osd_volume_error_line_append_int(&line, status->term_signal);
        osd_volume_error_line_append_text(&line, "\n");
        break;
    case OSD_VOLUME_PROC_ERR_EXIT_NONZERO:
        // Preserve exit code to correlate with wpctl stderr output
        osd_volume_error_line_append_query_prefix(&line, "wait", source, wpctl_path);
        osd_volume_error_line_append_text(&line, "child exited code=");
        osd_volume_error_line_append_int(&line, status->exit_code);
        osd_volume_error_line_append_text(&line, "\n");
        break;
    case OSD_VOLUME_PROC_ERR_NONE:
    default:
        // Defensive fallback for enum drift or unexpected status values
        osd_volume_error_line_append_query_prefix(&line, "unknown", source, wpctl_path);
        osd_volume_error_line_append_text(&line, "unknown proc error\n");
        break;
    }

    // Single line emit keeps canonical process diagnostics stable
    osd_volume_error_line_emit(err_stream, &line);
}

// Parse-phase errors include a sanitized preview of the offending output line
void osd_volume_write_parse_error(
    FILE *err_stream,
    const char *wpctl_path,
    OSDVolumePathSource source,
    const char *line,
    const OSDVolumeParseStatus *status
) {
    OSDVolumeErrorLine error_line;
    char preview[OSD_VOLUME_PARSE_PREVIEW_MAX];

    // Parse phase needs both raw line and parse status to form message
    if (err_stream == NULL || wpctl_path == NULL || line == NULL || status == NULL) {
        return;
    }

    // Preview is sanitized once and reused for each parse error variant
    osd_volume_error_sanitize_line_excerpt(line, preview, sizeof(preview));
    // Builder is reset after preview prep and before message assembly
    osd_volume_error_line_init(&error_line);

    // Output format stays stable for tooling and user diagnostics
    switch (status->error) {
    case OSD_VOLUME_PARSE_ERR_INVALID_ARG:
        osd_volume_error_line_append_query_prefix(&error_line, "parse", source, wpctl_path);
        osd_volume_error_line_append_text(&error_line, "invalid arguments\n");
        break;
    case OSD_VOLUME_PARSE_ERR_UNEXPECTED_FORMAT:
        // Keep offending line context bounded and printable only
        osd_volume_error_line_append_query_prefix(&error_line, "parse", source, wpctl_path);
        osd_volume_error_line_append_text(&error_line, "unexpected format line=\"");
        osd_volume_error_line_append_text(&error_line, preview);
        osd_volume_error_line_append_text(&error_line, "\"\n");
        break;
    case OSD_VOLUME_PARSE_ERR_INVALID_VALUE:
        // Numeric token failed validation or grammar checks
        osd_volume_error_line_append_query_prefix(&error_line, "parse", source, wpctl_path);
        osd_volume_error_line_append_text(&error_line, "invalid value line=\"");
        osd_volume_error_line_append_text(&error_line, preview);
        osd_volume_error_line_append_text(&error_line, "\"\n");
        break;
    case OSD_VOLUME_PARSE_ERR_UNEXPECTED_TAIL:
        // Extra bytes remained after expected tokens were consumed
        osd_volume_error_line_append_query_prefix(&error_line, "parse", source, wpctl_path);
        osd_volume_error_line_append_text(&error_line, "unexpected tail line=\"");
        osd_volume_error_line_append_text(&error_line, preview);
        osd_volume_error_line_append_text(&error_line, "\"\n");
        break;
    case OSD_VOLUME_PARSE_ERR_NONE:
    default:
        // Defensive fallback for enum drift or unexpected status values
        osd_volume_error_line_append_query_prefix(&error_line, "parse", source, wpctl_path);
        osd_volume_error_line_append_text(&error_line, "unknown parse error\n");
        break;
    }

    // Emit exactly one parse diagnostic line per failed parse attempt
    osd_volume_error_line_emit(err_stream, &error_line);
}
