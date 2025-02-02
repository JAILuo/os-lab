#include <kernel.h>
#include <klib.h>
#include <klib-macros.h>

struct cpu {
    int noff;
    int intena;
};

extern struct cpu cpus[];
#define mycpu (&cpus[cpu_current()])


