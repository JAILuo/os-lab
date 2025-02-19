#ifndef __COMMON_H
#define __COMMON_H

#include <kernel.h>
#include <klib.h>
#include <klib-macros.h>

struct cpu {
    int noff;
    int intena;
};

extern struct cpu cpus[];
#define mycpu (&cpus[cpu_current()])


//#define DEBUG
#ifdef DEBUG
#define debug_pf(fmt, args...) printf(fmt, ##args)
#else
#define debug_pf(fmt, args...) 
#endif


#endif
