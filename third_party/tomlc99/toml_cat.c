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
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct node_t node_t;
struct node_t {
  const char *key;
  toml_table_t *tab;
};

node_t stack[20];
int stacktop = 0;
int indent = 0;

static void prindent() {
  for (int i = 0; i < indent; i++)
    printf("  ");
}

static void print_string(const char *s) {
  int ok = 1;
  for (const char *p = s; *p && ok; p++) {
    int ch = *p;
    ok = isprint(ch) && ch != '"' && ch != '\\';
  }

  if (ok) {
    printf("\"%s\"", s);
    return;
  }

  int len = strlen(s);

  printf("\"");
  for (; len; len--, s++) {
    int ch = *s;
    if (isprint(ch) && ch != '"' && ch != '\\') {
      putchar(ch);
      continue;
    }

    switch (ch) {
    case 0x8:
      printf("\\b");
      continue;
    case 0x9:
      printf("\\t");
      continue;
    case 0xa:
      printf("\\n");
      continue;
    case 0xc:
      printf("\\f");
      continue;
    case 0xd:
      printf("\\r");
      continue;
    case '"':
      printf("\\\"");
      continue;
    case '\\':
      printf("\\\\");
      continue;
    default:
      printf("\\0x%02x", ch & 0xff);
      continue;
    }
  }
  printf("\"");
}

static void print_array(toml_array_t *arr);

static void print_timestamp(toml_datum_t d) {
  if (d.u.ts->year) {
    printf("%04d-%02d-%02d%s", *d.u.ts->year, *d.u.ts->month, *d.u.ts->day,
           d.u.ts->hour ? "T" : "");
  }
  if (d.u.ts->hour) {
    printf("%02d:%02d:%02d", *d.u.ts->hour, *d.u.ts->minute, *d.u.ts->second);
    if (d.u.ts->millisec) {
      printf(".%03d", *d.u.ts->millisec);
    }
    if (d.u.ts->z) {
      printf("%s", d.u.ts->z);
    }
  }
}

static void print_table(toml_table_t *curtab) {
  toml_datum_t d;
  int i;
  const char *key;
  toml_array_t *arr;
  toml_table_t *tab;

  for (i = 0; 0 != (key = toml_key_in(curtab, i)); i++) {

    if (0 != (arr = toml_array_in(curtab, key))) {
      prindent();
      printf("%s = [\n", key);
      indent++;
      print_array(arr);
      indent--;
      prindent();
      printf("],\n");
      continue;
    }

    if (0 != (tab = toml_table_in(curtab, key))) {
      stack[stacktop].key = key;
      stack[stacktop].tab = tab;
      stacktop++;
      prindent();
      printf("%s = {\n", key);
      indent++;
      print_table(tab);
      indent--;
      prindent();
      printf("},\n");
      stacktop--;
      continue;
    }

    d = toml_string_in(curtab, key);
    if (d.ok) {
      prindent();
      printf("%s = ", key);
      print_string(d.u.s);
      printf(",\n");
      free(d.u.s);
      continue;
    }

    d = toml_bool_in(curtab, key);
    if (d.ok) {
      prindent();
      printf("%s = %s,\n", key, d.u.b ? "true" : "false");
      continue;
    }

    d = toml_int_in(curtab, key);
    if (d.ok) {
      prindent();
      printf("%s = %" PRId64 ",\n", key, d.u.i);
      continue;
    }

    d = toml_double_in(curtab, key);
    if (d.ok) {
      prindent();
      printf("%s = %f,\n", key, d.u.d);
      continue;
    }

    d = toml_timestamp_in(curtab, key);
    if (d.ok) {
      prindent();
      printf("%s = ", key);
      print_timestamp(d);
      printf(",\n");
      free(d.u.ts);
      continue;
    }

    fflush(stdout);
    fprintf(stderr, "ERROR: unable to decode value in table\n");
    exit(1);
  }
}

static void print_array(toml_array_t *curarr) {
  toml_datum_t d;
  toml_array_t *arr;
  toml_table_t *tab;
  const int n = toml_array_nelem(curarr);

  for (int i = 0; i < n; i++) {

    if (0 != (arr = toml_array_at(curarr, i))) {
      prindent();
      printf("[\n");
      indent++;
      print_array(arr);
      indent--;
      prindent();
      printf("],\n");
      continue;
    }

    if (0 != (tab = toml_table_at(curarr, i))) {
      prindent();
      printf("{\n");
      indent++;
      print_table(tab);
      indent--;
      prindent();
      printf("},\n");
      continue;
    }

    d = toml_string_at(curarr, i);
    if (d.ok) {
      prindent();
      print_string(d.u.s);
      printf(",\n");
      free(d.u.s);
      continue;
    }

    d = toml_bool_at(curarr, i);
    if (d.ok) {
      prindent();
      printf("%s,\n", d.u.b ? "true" : "false");
      continue;
    }

    d = toml_int_at(curarr, i);
    if (d.ok) {
      prindent();
      printf("%" PRId64 ",\n", d.u.i);
      continue;
    }

    d = toml_double_at(curarr, i);
    if (d.ok) {
      prindent();
      printf("%f,\n", d.u.d);
      continue;
    }

    d = toml_timestamp_at(curarr, i);
    if (d.ok) {
      prindent();
      print_timestamp(d);
      printf(",\n");
      free(d.u.ts);
      continue;
    }

    fflush(stdout);
    fprintf(stderr, "ERROR: unable to decode value in array\n");
    exit(1);
  }
}

static void cat(FILE *fp) {
  char errbuf[200];

  toml_table_t *tab = toml_parse_file(fp, errbuf, sizeof(errbuf));
  if (!tab) {
    fprintf(stderr, "ERROR: %s\n", errbuf);
    return;
  }

  stack[stacktop].tab = tab;
  stack[stacktop].key = "";
  stacktop++;
  printf("{\n");
  indent++;
  print_table(tab);
  indent--;
  printf("}\n");
  stacktop--;

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
