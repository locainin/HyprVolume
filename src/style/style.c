#include "style/style.h"

#include "style/style_custom.h"
#include "style/style_icon.h"
#include "style/style_theme.h"

// Public facade delegates theme provider construction
GtkCssProvider *osd_style_build_provider(const OSDTheme *theme) {
    return osd_style_theme_build_provider(theme);
}

// Public facade delegates custom CSS loading and validation
bool osd_style_build_custom_provider(const char *path, GtkCssProvider **out_provider, FILE *err_stream) {
    return osd_style_custom_build_provider(path, out_provider, err_stream);
}

// Public facade delegates icon mapping logic
const char *osd_style_icon_name_for_state(const OSDVolumeState *state) {
    return osd_style_icon_name_for_state_internal(state);
}
