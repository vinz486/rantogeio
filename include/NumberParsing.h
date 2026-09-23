#ifndef NUMBER_PARSING_H
#define NUMBER_PARSING_H
#include <stdint.h>
#include <limits.h>
#include <ctype.h>

inline bool parseInteger(const char* text, int32_t& result) {
  while (isspace(static_cast<unsigned char>(*text))) ++text;
  const bool negative = *text == '-';
  if (*text == '-' || *text == '+') ++text;
  if (*text < '0' || *text > '9') return false;
  int64_t value = 0;
  while (*text >= '0' && *text <= '9') {
    value = value * 10 + (*text++ - '0');
    if (value > int64_t(INT32_MAX) + (negative ? 1 : 0)) return false;
  }
  while (isspace(static_cast<unsigned char>(*text))) ++text;
  if (*text) return false;
  result = int32_t(negative ? -value : value);
  return true;
}

// Exact hundredths, with either decimal separator; no float conversion.
inline bool parseTarget(const char* text, int32_t& result) {
  while (isspace(static_cast<unsigned char>(*text))) ++text;
  if (*text == '+') ++text;
  if (*text < '0' || *text > '9') return false;
  int64_t whole = 0;
  while (*text >= '0' && *text <= '9') {
    whole = whole * 10 + (*text++ - '0');
    if (whole > INT32_MAX / 100) return false;
  }
  int fraction = 0;
  if (*text == '.' || *text == ',') {
    ++text;
    if (*text < '0' || *text > '9') return false;
    fraction = (*text++ - '0') * 10;
    if (*text >= '0' && *text <= '9') fraction += *text++ - '0';
  }
  while (isspace(static_cast<unsigned char>(*text))) ++text;
  const int64_t value = whole * 100 + fraction;
  if (*text || value < 100 || value > INT32_MAX) return false;
  result = int32_t(value);
  return true;
}
#endif
