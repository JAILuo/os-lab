#ifndef __COMMON_H
#define __COMMON_H

#include <kernel.h>
#include <klib.h>
#include <klib-macros.h>
#include <os/spinlock.h>

struct cpu {
    int noff;
    int intena;
}__attribute__((aligned(64)));  // 对齐到缓存行


//#define DEBUG
#ifdef DEBUG
#define debug_pf(fmt, args...) \
    do { \
    spin_lock(&stdio_lock); \
    printf(fmt, ##args); \
    spin_unlock(&stdio_lock); \
    } while (0)
#else
#define debug_pf(fmt, args...) 
#endif


#endif
