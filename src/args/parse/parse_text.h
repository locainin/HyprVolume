#ifndef OSD_ARGS_PARSE_TEXT_H
#define OSD_ARGS_PARSE_TEXT_H

#include <stdbool.h>
#include <stddef.h>

// Copies source into destination with required NUL and explicit bounds check
bool osd_args_copy_text_bounded(
    char *destination,
    size_t destination_size,
    const char *source
);

#endif
