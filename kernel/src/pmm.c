#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include <common.h>
#include <spinlock.h>
#include <list.h>
#include <buddy.h>

#define PMMLOCKED 1
#define PMMUNLOCKED 0
#define SPIN_LIMIT 100000000
typedef int lock_t;
lock_t big_lock;

void lockinit(int *lock) {
    atomic_xchg(lock, PMMUNLOCKED);
}

// Low-profile version of lockdep
// it really does work
void spin_lock(int *lock) {
    // debug_pf("CPU #%d acquired Lock @ %s:%d\n", cpu_current(), __FILE__, __LINE__);
    int spin_cnt = 0;
    while(atomic_xchg(lock, PMMLOCKED) == PMMLOCKED) {
        if (spin_cnt++ > SPIN_LIMIT) {
            printf("Spin limit exceeded @ %s:%d\n",
                  __FILE__, __LINE__);
            panic("deadlock");
        }
    }
}

void spin_unlock(int *lock) {
    // debug_pf("CPU #%d release Lock @ %s:%d\n", cpu_current(), __FILE__, __LINE__);
    if (atomic_xchg(lock, PMMUNLOCKED) != PMMLOCKED) {
        printf("Warning: Unlocking an already unlocked lock @ %s:%d\n", __FILE__, __LINE__);
    }
}

// void spin_unlock(int *lock) {
//     panic_on(atomic_xchg(lock, PMMUNLOCKED) != PMMLOCKED, "lock is not acquired");
// }

/*----------------------------------------*/

void *slab_alloc(size_t size) {
    return NULL;
}

static void *kalloc(size_t size) {
    // TODO strange, deadlock will happen here, but not bellow...
    //spin_lock(&big_lock);

    //panic_on(size >= 16 * 1024 * 1024, "Allocations over 16MiB are not supported");
    //panic_on(size == 0, "0 Byte are not supported");

    if (size >= 16 * 1024 * 1024 || size == 0) return NULL;

    spin_lock(&big_lock);
    debug_pf("==========start alloc=========\n");
   
    size_t align_size = ROUNDUP(size, PAGESIZE);
    debug_pf("size: 0x%x  align_size: 0x%x\n", size, align_size);

    panic_on(align_size > 16 * 1024 * 1024, 
             "Allocations over 16MiB are not supported");

    // now for test, only >= 4KB
    // condititon should be > 4KB
    void *res = align_size >= PAGESIZE ?
        buddy_alloc(align_size) : slab_alloc(align_size);

    debug_pf("==========finish alloc=========\n");
    spin_unlock(&big_lock);
    return res;
}

static void range_check(void *ptr) {
    if (!IN_RANGE(ptr, heap)) {
        debug_pf("range_hea, ptr: 0x%x\n", ptr);
        spin_unlock(&big_lock);
        panic("should not free memory beyond heap.\n");
        return;
    }
    if ((uintptr_t)ptr < start_used) {
        debug_pf("ptr: 0x%x\n", ptr);
        spin_unlock(&big_lock);
        panic("should not free(cover) page meta_data and free_lists\n");
        return;
    }
}

static void kfree(void *ptr) {
    panic_on(ptr == NULL, "should not free NULL ptr\n");

    spin_lock(&big_lock);  // 加锁
    range_check(ptr);

    debug_pf("test free: 0x%x...\n", ptr);
    buddy_free(ptr);

    spin_unlock(&big_lock);  // 解锁
}

void init_pages();
void init_buddy() {
    init_pages();
}

static void pmm_init() {
    lockinit(&big_lock);

    uintptr_t pmsize = (
        (uintptr_t)heap.end - (uintptr_t)heap.start
    );

    printf(
        "Got %d MiB heap: [%p, %p)\n",
        pmsize >> 20, heap.start, heap.end
    );

    init_buddy();
}

MODULE_DEF(pmm) = {
    .init  = pmm_init,
    .alloc = kalloc,
    .free  = kfree,
};

