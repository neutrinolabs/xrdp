//
// Created by bc on 19.02.19.
//

#include <pthread.h>
#include <string.h>
#include "stacksize.h"

#define _GNU_SOURCE

void __set_thread_stack(void) {

#ifndef __GLIBC__
    // Increase default stack size for libmusl:
    pthread_attr_t a;
    memset(&a, 0, sizeof(pthread_attr_t));
    pthread_attr_setstacksize(&a, 8*1024*1024);  // 8MB as in glibc
    pthread_attr_setguardsize(&a, 4096);         // one page
    pthread_setattr_default_np(&a);
#endif

    return;
}