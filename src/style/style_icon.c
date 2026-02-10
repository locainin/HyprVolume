#include "style/style_icon.h"

// Maps runtime volume state to symbolic desktop icon names
// Return values are static string literals and are never heap-allocated
const char *osd_style_icon_name_for_state_internal(const OSDVolumeState *state) {
    // Null state falls back to muted icon for safe default rendering
    if (state == NULL) {
        return "audio-volume-muted-symbolic";
    }

    // Muted or zero volume uses muted icon regardless of bucket thresholds
    if (state->muted || state->volume_percent <= 0) {
        return "audio-volume-muted-symbolic";
    }

    // Buckets mirror common desktop volume icon transitions
    if (state->volume_percent < 34) {
        return "audio-volume-low-symbolic";
    }

    if (state->volume_percent < 67) {
        return "audio-volume-medium-symbolic";
    }

    return "audio-volume-high-symbolic";
}
