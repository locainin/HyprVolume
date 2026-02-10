#include "config/error.h"

#include <string.h>

// Writes a fixed error string to the configured stream
void osd_config_write_error_text(FILE *err_stream, const char *text) {
    size_t text_size = 0U;

    if (err_stream == NULL || text == NULL) {
        return;
    }
    text_size = strlen(text);
    if (text_size == 0U) {
        return;
    }
    (void)fwrite(text, sizeof(char), text_size, err_stream);
}

// Writes prefix + value + suffix without formatting directives
void osd_config_write_error_value_message(FILE *err_stream, const char *prefix, const char *value, const char *suffix) {
    osd_config_write_error_text(err_stream, prefix);
    osd_config_write_error_text(err_stream, value);
    osd_config_write_error_text(err_stream, suffix);
}

// Copies a C string into fixed storage and enforces NUL termination
bool osd_config_copy_cstr_bounded(char *destination, size_t destination_size, const char *source) {
    size_t index = 0U;

    if (destination == NULL || source == NULL || destination_size == 0U) {
        return false;
    }
    while (source[index] != '\0' && (index + 1U) < destination_size) {
        destination[index] = source[index];
        ++index;
    }
    if (source[index] != '\0') {
        destination[0] = '\0';
        return false;
    }
    destination[index] = '\0';
    return true;
}
