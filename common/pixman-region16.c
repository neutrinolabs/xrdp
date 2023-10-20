/*
 * Copyright © 2008 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Red Hat, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. Red Hat, Inc. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * RED HAT, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL RED HAT, INC. BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Soren Sandmann <sandmann@redhat.com>
 */

/* taken from pixman 0.34
   altered to compile without all of pixman */

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(HAVE_STDINT_H)
#include <stdint.h>
#endif
#include "pixman-region.h"

#if !defined(UINT32_MAX)
#define UINT32_MAX  (4294967295U)
#endif
#if !defined(INT16_MIN)
#define INT16_MIN   (-32767-1)
#endif
#if !defined(INT16_MAX)
#define INT16_MAX   (32767)
#endif

#define PIXMAN_EXPORT
#define FALSE 0
#define TRUE 1

#define MIN(x1, x2) ((x1) < (x2) ? (x1) : (x2))
#define MAX(x1, x2) ((x1) > (x2) ? (x1) : (x2))

typedef pixman_box16_t                  box_type_t;
typedef pixman_region16_data_t          region_data_type_t;
typedef pixman_region16_t               region_type_t;
typedef signed int                      overflow_int_t;

#define PREFIX(x) pixman_region##x

#define PIXMAN_REGION_MAX INT16_MAX
#define PIXMAN_REGION_MIN INT16_MIN

#define FUNC "func"

#define critical_if_fail(expr)

static int _pixman_log_error(const char *func, const char *format, ...)
{
    return 0;
}

#include "pixman-region.c"

