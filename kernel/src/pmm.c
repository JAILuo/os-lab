#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include <os/common.h>
#include <os/spinlock.h>
#include <os/list.h>
#include <os/buddy.h>

spinlock_t big_lock = spin_init("Big Kernel Lock");

/*----------------------------------------*/

void *slab_alloc(size_t size) {
    return NULL;
}

static void *kalloc(size_t size) {
    // TODO strange, deadlock will happen here, but not bellow...

    spin_lock(&big_lock);

    if (size >= 16 * 1024 * 1024 || size == 0) {
        spin_unlock(&big_lock);
        return NULL;
    }   
    debug_pf("==========start alloc=========\n");
   
    size_t align_size = ROUNDUP(size, PAGESIZE);
    debug_pf("size: 0x%x  align_size: 0x%x\n", size, align_size);

    panic_on(align_size > 16 * 1024 * 1024, 
             "Allocations over 16MiB are not supported");

    // now for test, only >= 4KB
    // condititon should be > 4KB
    void *res = align_size >= PAGESIZE ?
        buddy_alloc(align_size) : slab_alloc(align_size);

    spin_unlock(&big_lock);

    //spin_unlock(&area->lock);
    debug_pf("==========finish alloc=========\n");
    return res;
}

static void range_check(void *ptr) {
    if (!IN_RANGE(ptr, heap)) {
        printf("range_hea, ptr: 0x%x\n", ptr);
        panic("should not free memory not in heap.\n");
        return;
    }
    if ((uintptr_t)ptr < start_used) {
        printf("ptr: 0x%x\n", ptr);
        panic("should not free(cover) page meta_data and free_lists\n");
        return;
    }
}

static void kfree(void *ptr) {
    spin_lock(&big_lock);
    panic_on(ptr == NULL, "should not free NULL ptr\n");

    range_check(ptr);

    debug_pf("==========start free=========\n");

    buddy_free(ptr);

    debug_pf("==========end free=========\n");
    spin_unlock(&big_lock);
}

void init_pages();
static void init_buddy() {
    init_pages();
}

static void pmm_init() {
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

