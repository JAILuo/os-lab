#include <common.h>
#include <stdint.h>
#include <spinlock.h>
#include <stddef.h>

typedef struct freeblock_t {
    uintptr_t start;
    uintptr_t end;
    struct freeblock_t *next;
} freeblock_t;

freeblock_t *free_list = NULL;
//spinlock_t big_lock;

static void *kalloc(size_t size) {
    panic_on(size >= 16 * 1024 * 1024, "Allocations over 16MiB are not supported");

    size_t align_size = (size + 63) & (~63); // 对齐到64字节
    //spin_lock(&big_lock);

    freeblock_t *prev = NULL;
    freeblock_t *curr = free_list;

    while (curr != NULL) {
        // Use first fit strategies
        if ((curr->end - curr->start) >= align_size) {
            uintptr_t alloc_start = curr->start;
            uintptr_t alloc_end = curr->start + align_size;

            if (alloc_end != curr->end) { // 分割空闲块
                curr->start = alloc_end;
            } else { // 完全分配
                if (prev == NULL) {
                    free_list = curr->next;
                } else {
                    prev->next = curr->next;
                }
            }

            //spin_unlock(&big_lock);
            return (void *)alloc_start;
        }
        prev = curr;
        curr = curr->next;
    }

    //spin_unlock(&big_lock);
    return NULL; // 没有找到足够的空闲内存
}

static void kfree(void *ptr) {
    //spin_lock(&big_lock);

    freeblock_t *block = (freeblock_t *)ptr;
    block->start = (uintptr_t)ptr;
    block->end = (uintptr_t)ptr + ((freeblock_t *)ptr)->end;
    block->next = free_list;
    free_list = block;

    //spin_unlock(&big_lock);
}

static void pmm_init() {
    //spin_init(&big_lock);

    freeblock_t *block = (freeblock_t*)heap.start;
    block->start = (uintptr_t)heap.start;
    block->end = (uintptr_t)heap.end;
    block->next = NULL;
    free_list = block;

    uintptr_t pmsize = (
        (uintptr_t)heap.end
        - (uintptr_t)heap.start
    );

    printf(
        "Got %d MiB heap: [%p, %p)\n",
        pmsize >> 20, heap.start, heap.end
    );
}

MODULE_DEF(pmm) = {
    .init  = pmm_init,
    .alloc = kalloc,
    .free  = kfree,
};
