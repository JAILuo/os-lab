#ifndef __SPINLOCK__H
#define __SPINLOCK__H

#define UNLOCKED  0
#define LOCKED    1

typedef struct {
    const char *name;
    int status;
    //struct cpu *cpu;
} spinlock_t;

typedef int lock_t;

extern lock_t big_lock;

#define PMMLOCKED 1
#define PMMUNLOCKED 0
#define SPIN_LIMIT 100000000

#define spin_init(name_) \
    ((spinlock_t) { \
        .name = name_, \
        .status = UNLOCKED, \
        .cpu = NULL, \
    })
// void spin_lock(spinlock_t *lk);
// void spin_unlock(spinlock_t *lk);

void lockinit(int *lock);
void spin_lock(int *lock);
void spin_unlock(int *lock);

#endif
