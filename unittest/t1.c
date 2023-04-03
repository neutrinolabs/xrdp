#include "../toml.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(int argc, const char *argv[]) {
  char xxbuf[6], buf[6];
  int64_t xxcode, code;
  int xxsize;

  xxsize = 2, xxcode = 0x80;
  memcpy(xxbuf, "\xc2\x80", xxsize);
  assert(toml_ucs_to_utf8(xxcode, buf) == xxsize &&
         0 == memcmp(buf, xxbuf, xxsize));
  assert(toml_utf8_to_ucs(buf, xxsize, &code) == xxsize && code == xxcode);

  xxsize = 2, xxcode = 0x7ff;
  memcpy(xxbuf, "\xdf\xbf", xxsize);
  assert(toml_ucs_to_utf8(xxcode, buf) == xxsize &&
         0 == memcmp(buf, xxbuf, xxsize));
  assert(toml_utf8_to_ucs(buf, xxsize, &code) == xxsize && code == xxcode);

  xxsize = 3, xxcode = 0x800;
  memcpy(xxbuf, "\xe0\xa0\x80", xxsize);
  assert(toml_ucs_to_utf8(xxcode, buf) == xxsize &&
         0 == memcmp(buf, xxbuf, xxsize));
  assert(toml_utf8_to_ucs(buf, xxsize, &code) == xxsize && code == xxcode);

  xxsize = 3, xxcode = 0xfffd;
  memcpy(xxbuf, "\xef\xbf\xbd", xxsize);
  assert(toml_ucs_to_utf8(xxcode, buf) == xxsize &&
         0 == memcmp(buf, xxbuf, xxsize));
  assert(toml_utf8_to_ucs(buf, xxsize, &code) == xxsize && code == xxcode);

  xxsize = 4, xxcode = 0x10000;
  memcpy(xxbuf, "\xf0\x90\x80\x80", xxsize);
  assert(toml_ucs_to_utf8(xxcode, buf) == xxsize &&
         0 == memcmp(buf, xxbuf, xxsize));
  assert(toml_utf8_to_ucs(buf, xxsize, &code) == xxsize && code == xxcode);

  xxsize = 4, xxcode = 0x1fffff;
  memcpy(xxbuf, "\xf7\xbf\xbf\xbf", xxsize);
  assert(toml_ucs_to_utf8(xxcode, buf) == xxsize &&
         0 == memcmp(buf, xxbuf, xxsize));
  assert(toml_utf8_to_ucs(buf, xxsize, &code) == xxsize && code == xxcode);

  xxsize = 5, xxcode = 0x200000;
  memcpy(xxbuf, "\xf8\x88\x80\x80\x80", xxsize);
  assert(toml_ucs_to_utf8(xxcode, buf) == xxsize &&
         0 == memcmp(buf, xxbuf, xxsize));
  assert(toml_utf8_to_ucs(buf, xxsize, &code) == xxsize && code == xxcode);

  xxsize = 5, xxcode = 0x3ffffff;
  memcpy(xxbuf, "\xfb\xbf\xbf\xbf\xbf", xxsize);
  assert(toml_ucs_to_utf8(xxcode, buf) == xxsize &&
         0 == memcmp(buf, xxbuf, xxsize));
  assert(toml_utf8_to_ucs(buf, xxsize, &code) == xxsize && code == xxcode);

  xxsize = 6, xxcode = 0x4000000;
  memcpy(xxbuf, "\xfc\x84\x80\x80\x80\x80", xxsize);
  assert(toml_ucs_to_utf8(xxcode, buf) == xxsize &&
         0 == memcmp(buf, xxbuf, xxsize));
  assert(toml_utf8_to_ucs(buf, xxsize, &code) == xxsize && code == xxcode);

  xxsize = 6, xxcode = 0x7fffffff;
  memcpy(xxbuf, "\xfd\xbf\xbf\xbf\xbf\xbf", xxsize);
  assert(toml_ucs_to_utf8(xxcode, buf) == xxsize &&
         0 == memcmp(buf, xxbuf, xxsize));
  assert(toml_utf8_to_ucs(buf, xxsize, &code) == xxsize && code == xxcode);

  return 0;
}
