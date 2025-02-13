#ifndef __BUDDY__H
#define __BUDDY__H

#include <list.h>
#include <common.h>
#include <stdbool.h>
#include <stdint.h>

#define MAX_ORDER (10 + 1)
#define PAGESIZE (4 * 1024)
#define MAX_PAGESIZE ((1 << (MAX_ORDER - 1)) * PAGESIZE)

#define HEAP_SIZE (((uintptr_t)heap.end) - ((uintptr_t)heap.start))
#define TOTAL_PAGES (HEAP_SIZE / PAGESIZE)

struct page {
    struct list_head buddy_list;
    unsigned int order;         // 块阶数（0~10）
    bool used;                  // 是否被使用
    bool is_slab;
    struct page *compound_head;
};

struct free_area {
    //struct list_head *head;
    struct list_head head;
    unsigned long nr_free;
};

extern struct free_area *free_lists;
//extern struct free_area free_lists[MAX_ORDER];
extern uintptr_t start_used;

#define pages_base ((struct page *)(heap.start))

// 假设 page 结构体数组起始地址为 pages_base
#define page_to_pfn(page) ((unsigned long)((page) - pages_base))
#define pfn_to_page(pfn) (&pages_base[(pfn)])

// static inline struct page *pfn_to_page(unsigned long pfn) {
//     // return (struct page *)heap.start + pfn;
//     return (struct page *)((uintptr_t)heap.start + pfn * sizeof(struct page));
// }
// 
// static inline unsigned long page_to_pfn(struct page *page) {
//     return ((unsigned long)((uintptr_t)page - (uintptr_t)heap.start) / sizeof(struct page));
// }

static inline unsigned long ptr_to_pfn(void *ptr) {
    return ((uintptr_t) ptr - (uintptr_t) heap.start) / PAGESIZE;
}

static inline int get_order(size_t size) {
    int order = 0;
    while ((1UL << order) * PAGESIZE < size) order++;
    return order;
}

void *buddy_alloc(size_t size);
void buddy_free(void *ptr);

#endif
