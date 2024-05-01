#ifndef UTIL_H__
#define UTIL_H__

#include "common.h"

char screenCode_to_Ascii(byte screenCode);
byte Ascii_to_screenCode(char ascii);

static inline bool is_base64(unsigned char c) {
  return (isalnum(c) || (c == '+') || (c == '/'));
}

String my_base64_encode(char* buf, int bufLen);
String my_base64_decode(String const& encoded_string);
byte checksum(byte data[], int datasize);
int x2i(char* s);
String getValue(String data, char separator, int index);
#endif // UTIL_H__
