#include <common.h>
#include <stdint.h>
#include <spinlock.h>
#include <stddef.h>

#define PMMLOCKED 1
#define PMMUNLOCKED 0

#define PAGE_SIZE 4096
#define THRESHOLD (4 * 1024) // 示例阈值，可根据需要调整
#define ALIGNMENT 64

typedef int lock_t;

void lockinit(int *lock) {
    atomic_xchg(lock, PMMUNLOCKED);
}

void spin_lock(int *lock) {
    while(atomic_xchg(lock, PMMLOCKED) == PMMLOCKED) {;}
}

void spin_unlock(int *lock) {
    panic_on(atomic_xchg(lock, PMMUNLOCKED) != PMMLOCKED, "lock is not acquired");
}

typedef struct header {
    size_t size;
    bool is_page;
} header_t;

typedef struct freeblock {
    uintptr_t start;
    uintptr_t end;
    struct freeblock *next;
    bool is_used;
    bool is_page; // 标记是否为页块
} freeblock_t;

static freeblock_t *free_list = NULL;
lock_t big_lock;

static inline size_t align_up(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

static size_t calculate_alignment(size_t size) {
    if (size == 0) {
        return 1; 
    }

    size_t alignment = 1;
    while (alignment < size) {
        alignment <<= 1;
    }
    return alignment;
}


static void *kalloc(size_t size) {
    panic_on(size >= 16 * 1024 * 1024, "Allocations over 16MiB are not supported");

    spin_lock(&big_lock);

    size_t align_size;
    bool is_page = false;
    size_t total_size;

    // 事实上，我这里区分是没什么意义的
    // 我应该使用的是别的页分配器，而上面的是别的内存块分配器
    // 我这里使用的还是最简陋的纯块分配（可大可小的块几Byte 到4KB往上）
    // 所以is_page 这里没意义
    // 就当作为之后的 buddy system 和 slab 实现开个头。
    if (size <= THRESHOLD - sizeof(header_t)) {
        align_size = size;
        total_size = calculate_alignment(align_size + sizeof(header_t));
    } else {
        align_size = size;
        total_size = align_size + sizeof(header_t);
        total_size = align_up(total_size, PAGE_SIZE);
        is_page = true;
    }

    freeblock_t *curr = free_list;
    freeblock_t *prev = NULL;

    // first fit strategy
    freeblock_t *choosed = NULL;
    freeblock_t *choosed_prev = NULL;
    // size_t choosed_fit = SIZE_MAX;
    while (curr != NULL) {

        uintptr_t alloc_start = curr->start;
        uintptr_t alloc_end = alloc_start + total_size;
        size_t remaining_size = curr->end - curr->start;

        // 修正条件判断：块已使用 或 无法容纳请求时跳过
        if (curr->is_used || alloc_end > curr->end || remaining_size < total_size) {
            prev = curr;
            curr = curr->next;
            continue;
        }

        // 确保当前块足够大，否则应已跳过
        if (alloc_end > curr->end) {
            printf("alloc_start: 0x%x alloc_end: 0x%x "
                   "curr->start: 0x%x curr->end: 0x%x "
                   "remaining_size: 0x%x\n",
                   alloc_start, alloc_end, curr->start,
                   curr->end, remaining_size);
            panic("allocation logic error: block size mismatch");
        }

        // size_t remaining = curr->end - alloc_end;
        // if (remaining < choosed_fit) {
            //choosed_fit = remaining;
            choosed = curr;
            choosed_prev = prev;
        // }
        break;
    }

    if (choosed != NULL) {
        uintptr_t alloc_start = choosed->start;
        uintptr_t alloc_end = alloc_start + total_size;

        // set header info
        header_t *hptr = (header_t*)alloc_start;
        hptr->is_page = is_page;
        hptr->size = total_size;

        choosed->is_used = true;

        // handle remaining free space, update free_list
        if (alloc_end < choosed->end) {
            freeblock_t *remaining = (freeblock_t *)(alloc_end);
            remaining->start = alloc_end;
            remaining->end = choosed->end;
            //remaining->end = curr->end;
            remaining->is_used = false;
            remaining->next = choosed->next;

            if (choosed_prev)
                choosed_prev->next = remaining;
            else
                free_list = remaining;
        } else {
            // 移除当前块
            if (choosed_prev)
                choosed_prev->next = choosed->next;
            else
                free_list = choosed->next;
        }
        spin_unlock(&big_lock);
        return (void *)(alloc_start + sizeof(header_t));
    }
    
    spin_unlock(&big_lock);
    panic("no......");
    return NULL;
}

static void kfree(void *ptr) {
    if (!ptr) return;

    spin_lock(&big_lock);
    header_t *hdr = (header_t *)((uintptr_t)ptr - sizeof(header_t));
    uintptr_t start = (uintptr_t)hdr;
    uintptr_t end = start + sizeof(header_t) + hdr->size;

    // 创建新空闲块并标记类型
    freeblock_t *block = (freeblock_t *)start;
    block->start = start;
    block->end = end;
    block->is_page = hdr->is_page;
    block->is_used = false;
    block->next = free_list;
    free_list = block;

    // 合并相邻块
    // freeblock_t *curr = free_list;
    // while (curr) {
    //     printf("ddddddddddddddd\n");
    //     freeblock_t *next = curr->next;
    //     if (next && curr->end == next->start && curr->is_page == next->is_page) {
    //         curr->end = next->end;
    //         curr->next = next->next;
    //         // 继续检查以合并更多块
    //     } else {
    //         curr = curr->next;
    //     }
    // }

    spin_unlock(&big_lock);
}

static void pmm_init() {
    lockinit(&big_lock);

    uintptr_t pmsize = (
        (uintptr_t)heap.end - (uintptr_t)heap.start
    );

    freeblock_t *block = (freeblock_t *)heap.start;
    block->start = (uintptr_t)heap.start;
    block->end = (uintptr_t)heap.end;
    block->is_used = false;
    block->next = NULL;
    free_list = block;

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

