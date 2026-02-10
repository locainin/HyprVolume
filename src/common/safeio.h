#ifndef HYPRVOLUME_COMMON_SAFEIO_H
#define HYPRVOLUME_COMMON_SAFEIO_H

#include <stdbool.h>
#include <stdio.h>

// Writes a NUL-terminated string to stream
bool osd_io_write_text(FILE *stream, const char *text);

// Writes a NUL-terminated string followed by newline
bool osd_io_write_line(FILE *stream, const char *text);

// Writes a signed integer in base-10 text form
bool osd_io_write_long_long(FILE *stream, long long value);

// Writes an unsigned integer in base-10 text form
bool osd_io_write_unsigned_long_long(FILE *stream, unsigned long long value);

#endif
