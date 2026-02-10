#include "parse_text.h"

#include <string.h>

bool osd_args_copy_text_bounded(
    char *destination,
    size_t destination_size,
    const char *source
) {
    size_t source_len = 0U;
    size_t index = 0U;

    // Null and zero-size checks keep all call sites predictable
    if (destination == NULL || destination_size == 0U || source == NULL) {
        return false;
    }

    // Copy only when source fully fits with trailing NUL byte
    source_len = strlen(source);
    if (source_len >= destination_size) {
        return false;
    }

    // Manual copy keeps behavior identical across toolchains
    for (index = 0U; index < source_len; index++) {
        destination[index] = source[index];
    }

    // Always terminate destination even for empty input strings
    destination[source_len] = '\0';
    return true;
}
