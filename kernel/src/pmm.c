#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include <common.h>
#include <spinlock.h>
#include <list.h>
#include <buddy.h>

#define PMMLOCKED 1
#define PMMUNLOCKED 0

typedef int lock_t;
lock_t big_lock;

void lockinit(int *lock) {
    atomic_xchg(lock, PMMUNLOCKED);
}

void spin_lock(int *lock) {
    while(atomic_xchg(lock, PMMLOCKED) == PMMLOCKED) {;}
}

void spin_unlock(int *lock) {
    panic_on(atomic_xchg(lock, PMMUNLOCKED) != PMMLOCKED, "lock is not acquired");
}
/*----------------------------------------*/

void *slab_alloc(size_t size) {
    return NULL;
}

static void *kalloc(size_t size) {
    spin_lock(&big_lock);
    if (size > 16 * 1024 * 1024) return NULL; // Allocations over 16MiB are not supported

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

static void kfree(void *ptr) {
    spin_lock(&big_lock);  // 加锁
    debug_pf("test free: 0x%x...\n", ptr);
    // 释放内存的逻辑
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

