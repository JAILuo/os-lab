#include <stdbool.h>

#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <lock.h>

// This is a ported version of spin-lock
// from xv6-riscv to AbstractMachine:
// https://github.com/mit-pdos/xv6-riscv

static void push_off();
static void pop_off();
static bool holding(lock_t *lk);

static struct stdio_cpu cpus[16];
lock_t stdio_lock = spin_init("stdio Lock");

void lock(lock_t *lk) {
    // Disable interrupts to avoid deadlock.
    push_off();

    // This is a deadlock.
    if (holding(lk)) {
        panic("deadlock");
    }

    // This our main body of spin lock.
    int got;
    int spin_cnt = 0;
    do {
        got = atomic_xchg(&lk->status, LOCKED);
        if (spin_cnt++ > SPIN_LIMIT) {
            panic("deadlock detected");
        }
    } while (got != UNLOCKED);

    lk->cpu = mycpu;
}

void unlock(lock_t *lk) {
    if (!holding(lk)) {
        panic("deadlock");
    }

    lk->cpu = NULL;
    if (atomic_xchg(&lk->status, UNLOCKED) != LOCKED) {
        panic("invalid unlock of lock");
    }

    pop_off();
}

// Check whether this cpu is holding the lock.
// Interrupts must be off.
static bool holding(lock_t *lk) {
    return (
        lk->status == LOCKED &&
        lk->cpu == &cpus[cpu_current()]
    );
}

// push_off/pop_off are like intr_off()/intr_on()
// except that they are matched:
// it takes two pop_off()s to undo two push_off()s.
// Also, if interrupts are initially off, then
// push_off, pop_off leaves them off.
static void push_off(void) {
    int old = ienabled();
    struct stdio_cpu *c = mycpu;

    iset(false);
    if (c->noff == 0) {
        c->intena = old;
    }
    c->noff += 1;
}

static void pop_off(void) {
    struct stdio_cpu *c = mycpu;

    // Never enable interrupt when holding a lock.
    if (ienabled()) {
        panic("pop_off - interruptible");
    }
    
    if (c->noff < 1) {
        panic("pop_off");
    }

    c->noff -= 1;
    if (c->noff == 0 && c->intena) {
        iset(true);
    }
}


