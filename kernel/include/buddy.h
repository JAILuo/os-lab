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

struct page {
    struct list_head buddy_list;
    unsigned int order;         // 块阶数（0~10）
    bool used;                  // 是否被使用
    bool is_slab;
    struct page *compound_head;
};

struct free_area {
    struct list_head *head;
    unsigned long nr_free;
};

extern struct free_area *free_lists;


static inline struct page *pfn_to_page(unsigned long pfn) {
    // return (struct page *)heap.start + pfn;
    return (struct page *)((uintptr_t)heap.start + pfn * sizeof(struct page));
}

static inline unsigned long page_to_pfn(struct page *page) {
    return ((unsigned long)((uintptr_t)page - (uintptr_t)heap.start) / sizeof(struct page));
}

static inline int get_order(size_t size) {
    int order = 0;
    while ((1UL << order) * PAGESIZE < size) order++;
    return order;
}

void *buddy_alloc(size_t size);

#endif
