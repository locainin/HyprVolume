#ifndef HYPRVOLUME_SYSTEM_VOLUME_ERROR_INTERNAL_H
#define HYPRVOLUME_SYSTEM_VOLUME_ERROR_INTERNAL_H

#include "system/volume/volume_path.h"
#include <stddef.h>
#include <stdio.h>

#define OSD_VOLUME_PARSE_PREVIEW_MAX 96U
#define OSD_VOLUME_ERROR_TEXT_MAX 768U

// Shared bounded line builder for canonical diagnostics
// This stays private to the volume_error translation units
// and is not part of the public system API
typedef struct {
  // Fully composed log line buffer
  char text[OSD_VOLUME_ERROR_TEXT_MAX];
  // Number of bytes currently filled in text
  size_t used;
} OSDVolumeErrorLine;

// Maps source enum to stable text used in diagnostics
const char *osd_volume_error_path_source_label(OSDVolumePathSource source);

// Returns safe detail fallback for null pointers
const char *osd_volume_error_safe_detail_text(const char *detail);

// Produces bounded single-line preview text
void osd_volume_error_sanitize_line_excerpt(const char *line, char *out_preview, size_t out_preview_size);

// Builder lifecycle and append helpers
void osd_volume_error_line_init(OSDVolumeErrorLine *line);
void osd_volume_error_line_append_char(OSDVolumeErrorLine *line, char value);
void osd_volume_error_line_append_text(OSDVolumeErrorLine *line, const char *text);
void osd_volume_error_line_append_int(OSDVolumeErrorLine *line, int value);
void osd_volume_error_line_emit(FILE *err_stream, const OSDVolumeErrorLine *line);

// Appends shared query prefix fragment used by proc/parse errors
void osd_volume_error_line_append_query_prefix(OSDVolumeErrorLine *line, const char *phase,
                                               OSDVolumePathSource source, const char *wpctl_path);

#endif
