/**
 * Copyright (C) 2017 Koichiro Iwao, aka, metalefty <meta@vmeta.jp>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Test code on common/os_calls.h
 */

#include <stdio.h>
#include <assert.h>
#include <config_ac.h>
#include "os_calls.h"

/**
 * Test if overflow doesn't occur in g_time1(), g_time2(), g_time3()
 */
int
main(void)
{
#if !defined(_WIN32) /* This test is not intended to be run under Win32 */
    uint64_t gTime1, gTime2, gTime3;

    gTime1 = g_time1();
    gTime2 = g_time2();
    gTime3 = g_time3();

    printf("g_time1()        = %16"PRIu64" [ sec] (epoch time)\n",
            gTime1);
    printf("g_time1() * 1000 = %16"PRIu64" [msec] (epoch time)\n",
            gTime1 * 1000);
    printf("g_time2()        = %16"PRIu64" [msec] (since machine was started)\n",
            gTime2);
    printf("g_time3()        = %16"PRIu64" [msec] (epoch time in msec)\n",
            gTime3);

    printf("\n");
    printf("Testing...\n");

    /**
     * running time never greater than epoch time
     */
    assert(gTime1 * 1000 >  gTime2);

    /**
     * g_time3() is always greater than equal to g_time1() * 1000
     *
     * e.g. 1522427663.265 * 1000 > 1522427663 * 1000
     */
    assert(gTime3 >=  gTime1 * 1000);

    printf("All tests passed!\n");
#endif

    return 0;
}
