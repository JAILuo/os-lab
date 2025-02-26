#ifndef __LOCK_H
#define __LOCK_H

#define UNLOCKED  0
#define LOCKED    1
struct stdio_cpu {
    int noff;
    int intena;
}__attribute__((aligned(64)));  // 对齐到缓存行
                                
typedef struct {
    const char *name;
    int status;
    struct stdio_cpu *cpu;
} lock_t;

#define SPIN_LIMIT 1000000
#define spin_init(name_) \
    ((lock_t) { \
        .name = name_, \
        .status = UNLOCKED, \
        .cpu = NULL, \
    })
void lock(lock_t *lk);
void unlock(lock_t *lk);

#define mycpu (&cpus[cpu_current()])



#endif
