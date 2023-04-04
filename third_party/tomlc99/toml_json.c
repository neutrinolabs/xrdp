/*
  MIT License

  Copyright (c) 2017 CK Tan
  https://github.com/cktan/tomlc99

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/
#ifdef NDEBUG
#undef NDEBUG
#endif

#include "toml.h"
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

static void print_escape_string(const char *s) {
  for (; *s; s++) {
    int ch = *s;
    switch (ch) {
    case '\b':
      printf("\\b");
      break;
    case '\t':
      printf("\\t");
      break;
    case '\n':
      printf("\\n");
      break;
    case '\f':
      printf("\\f");
      break;
    case '\r':
      printf("\\r");
      break;
    case '"':
      printf("\\\"");
      break;
    case '\\':
      printf("\\\\");
      break;
    default:
      printf("%c", ch);
      break;
    }
  }
}

static void print_raw(const char *s) {
  char *sval;
  int64_t ival;
  int bval;
  double dval;
  toml_timestamp_t ts;
  char dbuf[100];

  if (0 == toml_rtos(s, &sval)) {
    printf("{\"type\":\"string\",\"value\":\"");
    print_escape_string(sval);
    printf("\"}");
    free(sval);
  } else if (0 == toml_rtoi(s, &ival)) {
    printf("{\"type\":\"integer\",\"value\":\"%" PRId64 "\"}", ival);
  } else if (0 == toml_rtob(s, &bval)) {
    printf("{\"type\":\"bool\",\"value\":\"%s\"}", bval ? "true" : "false");
  } else if (0 == toml_rtod_ex(s, &dval, dbuf, sizeof(dbuf))) {
    printf("{\"type\":\"float\",\"value\":\"%s\"}", dbuf);
  } else if (0 == toml_rtots(s, &ts)) {
    char millisec[10];
    if (ts.millisec)
      sprintf(millisec, ".%03d", *ts.millisec);
    else
      millisec[0] = 0;
    if (ts.year && ts.hour) {
      printf("{\"type\":\"datetime\",\"value\":\"%04d-%02d-%02dT%02d:%02d:%02d%"
             "s%s\"}",
             *ts.year, *ts.month, *ts.day, *ts.hour, *ts.minute, *ts.second,
             millisec, (ts.z ? ts.z : ""));
    } else if (ts.year) {
      printf("{\"type\":\"date\",\"value\":\"%04d-%02d-%02d\"}", *ts.year,
             *ts.month, *ts.day);
    } else if (ts.hour) {
      printf("{\"type\":\"time\",\"value\":\"%02d:%02d:%02d%s\"}", *ts.hour,
             *ts.minute, *ts.second, millisec);
    }
  } else {
    fprintf(stderr, "unknown type\n");
    exit(1);
  }
}

static void print_array(toml_array_t *arr);
static void print_table(toml_table_t *curtab) {
  int i;
  const char *key;
  const char *raw;
  toml_array_t *arr;
  toml_table_t *tab;

  printf("{");
  for (i = 0; 0 != (key = toml_key_in(curtab, i)); i++) {

    printf("%s\"", i > 0 ? "," : "");
    print_escape_string(key);
    printf("\":");

    if (0 != (raw = toml_raw_in(curtab, key))) {
      print_raw(raw);
    } else if (0 != (arr = toml_array_in(curtab, key))) {
      print_array(arr);
    } else if (0 != (tab = toml_table_in(curtab, key))) {
      print_table(tab);
    } else {
      abort();
    }
  }
  printf("}");
}

static void print_table_array(toml_array_t *curarr) {
  int i;
  toml_table_t *tab;

  printf("[");
  for (i = 0; 0 != (tab = toml_table_at(curarr, i)); i++) {
    printf("%s", i > 0 ? "," : "");
    print_table(tab);
  }
  printf("]");
}

static void print_array(toml_array_t *curarr) {
  toml_array_t *arr;
  const char *raw;
  int i;

  if (toml_array_kind(curarr) == 't') {
    print_table_array(curarr);
    return;
  }

  printf("{\"type\":\"array\",\"value\":[");
  switch (toml_array_kind(curarr)) {

  case 'v':
    for (i = 0; 0 != (raw = toml_raw_at(curarr, i)); i++) {
      printf("%s", i > 0 ? "," : "");
      print_raw(raw);
    }
    break;

  case 'a':
    for (i = 0; 0 != (arr = toml_array_at(curarr, i)); i++) {
      printf("%s", i > 0 ? "," : "");
      print_array(arr);
    }
    break;

  default:
    break;
  }
  printf("]}");
}

static void cat(FILE *fp) {
  char errbuf[200];

  toml_table_t *tab = toml_parse_file(fp, errbuf, sizeof(errbuf));
  if (!tab) {
    fprintf(stderr, "ERROR: %s\n", errbuf);
    exit(1);
  }

  print_table(tab);
  printf("\n");

  toml_free(tab);
}

int main(int argc, const char *argv[]) {
  int i;
  if (argc == 1) {
    cat(stdin);
  } else {
    for (i = 1; i < argc; i++) {

      FILE *fp = fopen(argv[i], "r");
      if (!fp) {
        fprintf(stderr, "ERROR: cannot open %s: %s\n", argv[i],
                strerror(errno));
        exit(1);
      }
      cat(fp);
      fclose(fp);
    }
  }
  return 0;
}
