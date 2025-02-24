#include <os/common.h>
#include <os/spinlock.h>

// This is a ported version of spin-lock
// from xv6-riscv to AbstractMachine:
// https://github.com/mit-pdos/xv6-riscv

static struct cpu cpus[16];
#define mycpu (&cpus[cpu_current()])

void push_off();
void pop_off();
bool holding(spinlock_t *lk);

#define new
#ifdef new
void safe_printf(const char *format, ...) {
    spin_lock(&big_lock);

    va_list args;
    va_start(args, format);
    printf(format, args);
    va_end(args);

    spin_unlock(&big_lock);
}
#else 
void safe_printf(const char *format, ...) {}
#endif

void spin_lock(spinlock_t *lk) {
    //printf("CPU #%d acquired Lock @ %s:%d\n", cpu_current(), __FILE__, __LINE__);
    // Disable interrupts to avoid deadlock.
    push_off();

    // This is a deadlock.
    if (holding(lk)) {
        safe_printf("acquire %s\n", lk->name);
        panic("deadlock");
        //panic("acquire %s", lk->name);
    }

    // This our main body of spin lock.
    int got;
    int spin_cnt = 0;
    do {
        got = atomic_xchg(&lk->status, LOCKED);
        if (spin_cnt++ > SPIN_LIMIT) {
            safe_printf("Spin count exceeded for lock %s @ %s:%d\n",
                        lk->name, __FILE__, __LINE__);
            panic("deadlock detected");
        }
    } while (got != UNLOCKED);

    lk->cpu = mycpu;
}

void spin_unlock(spinlock_t *lk) {
    //printf("CPU #%d release Lock @ %s:%d\n", cpu_current(), __FILE__, __LINE__);
    if (!holding(lk)) {
        safe_printf("acquire %s\n", lk->name);
        panic("deadlock");
        //panic("release %s", lk->name);
    }

    lk->cpu = NULL;
    if (atomic_xchg(&lk->status, UNLOCKED) != LOCKED) {
        safe_printf("Trying to unlock an unlocked lock %s @ %s:%d\n", 
                    lk->name, __FILE__, __LINE__);
        panic("invalid unlock of lock");
    }

    pop_off();
}

// Check whether this cpu is holding the lock.
// Interrupts must be off.
bool holding(spinlock_t *lk) {
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
void push_off(void) {
    int old = ienabled();
    struct cpu *c = mycpu;

    iset(false);
    if (c->noff == 0) {
        c->intena = old;
    }
    c->noff += 1;
}

void pop_off(void) {
    struct cpu *c = mycpu;

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

