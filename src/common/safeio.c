#include "common/safeio.h"

#include <string.h>

bool osd_io_write_text(FILE *stream, const char *text) {
  size_t text_length = 0U;

  // Null stream or text is always a hard failure
  if (stream == NULL || text == NULL) {
    return false;
  }

  // Empty writes are treated as success for simpler call sites
  text_length = strlen(text);
  if (text_length == 0U) {
    return true;
  }

  // fwrite return must match full input length
  return fwrite(text, 1U, text_length, stream) == text_length;
}

bool osd_io_write_line(FILE *stream, const char *text) {
  // Write text first, then line terminator for predictable output shape
  if (!osd_io_write_text(stream, text)) {
    return false;
  }

  return fputc('\n', stream) != EOF;
}

bool osd_io_write_long_long(FILE *stream, long long value) {
  char reversed[32];
  size_t digit_count = 0U;
  unsigned long long magnitude = 0ULL;
  bool negative = false;

  if (stream == NULL) {
    return false;
  }

  // Convert signed value to magnitude without undefined behavior on LLONG_MIN
  if (value < 0LL) {
    negative = true;
    magnitude = (unsigned long long)(-(value + 1LL)) + 1ULL;
  } else {
    magnitude = (unsigned long long)value;
  }

  // Build decimal digits in reverse order
  do {
    unsigned int digit = (unsigned int)(magnitude % 10ULL);

    if (digit_count >= sizeof(reversed)) {
      return false;
    }

    reversed[digit_count] = (char)('0' + digit);
    digit_count++;
    magnitude /= 10ULL;
  } while (magnitude > 0ULL);

  // Signed output writes '-' before replaying digits
  if (negative && fputc('-', stream) == EOF) {
    return false;
  }

  // Replay reversed digits into forward decimal text
  while (digit_count > 0U) {
    digit_count--;
    if (fputc(reversed[digit_count], stream) == EOF) {
      return false;
    }
  }

  return true;
}

bool osd_io_write_unsigned_long_long(FILE *stream, unsigned long long value) {
  char reversed[32];
  size_t digit_count = 0U;
  unsigned long long magnitude = value;

  if (stream == NULL) {
    return false;
  }

  // Build decimal digits in reverse order
  do {
    unsigned int digit = (unsigned int)(magnitude % 10ULL);

    if (digit_count >= sizeof(reversed)) {
      return false;
    }

    reversed[digit_count] = (char)('0' + digit);
    digit_count++;
    magnitude /= 10ULL;
  } while (magnitude > 0ULL);

  // Replay reversed digits into forward decimal text
  while (digit_count > 0U) {
    digit_count--;
    if (fputc(reversed[digit_count], stream) == EOF) {
      return false;
    }
  }

  return true;
}
