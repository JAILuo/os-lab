#include "os/spinlock.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <os/common.h>
#include <os/buddy.h>
#include <os/list.h>

bool range_check_addr(void *ptr);

// TODO: should use is_slab after starting slab...
void print_free_lists_nr_free() {
    for (int i = MAX_ORDER - 1; i >= 0; i--) {
        printf("free_lists[%d].nr_free: %d\n",
                 i, free_lists[i].nr_free);
    }
    printf("=======================\n");
}

static inline
void remove_from_free_list(struct free_area *area, struct page *page) {
    panic_on(area->nr_free == 0, "should not remove empty free_lists");
    list_del(&page->buddy_list);
    area->nr_free--;
}

static inline struct page *get_page_from_free_area(int order) {
	return list_first_entry_or_null(&free_lists[order].head,
					struct page, buddy_list);
}

static void *allocate_block(int order) {
    struct free_area *area = &free_lists[order];

    //spin_lock(&area->lock);
    if (list_empty(&area->head) || area->nr_free == 0) {
        //spin_unlock(&area->lock);
        return NULL;
    }

    struct page *block_page = get_page_from_free_area(order);
    if (block_page == NULL) {
        //spin_unlock(&area->lock);
        return NULL;
    }

    unsigned long pfn = page_to_pfn(block_page);
    uintptr_t ptr = (uintptr_t)pfn_to_ptr(pfn);

    panic_on(pfn < pfn_start, "pfn error");
    panic_on(block_page->used == true, "block in free_area should not be used");
    panic_on(area->nr_free == 0, "free_area should have block");
    panic_on(ptr < start_used, "addr should not less than start_used");

    remove_from_free_list(area, block_page);

    block_page->used = true;
    block_page->order = order;

    //spin_unlock(&area->lock);
    return block_page;
}

static inline 
void add_buddy_to_freelist(struct page *buddy_page, int order) {
    struct free_area *area = &free_lists[order];
    list_add(&buddy_page->buddy_list, (struct list_head *)&free_lists[order]);
    area->nr_free++;
}


// static inline unsigned long find_buddy_pfn(unsigned long page_pfn, unsigned int order) {
//     return page_pfn ^ (1 << order);
// }
// static bool page_is_buddy(struct page *page1, struct page *page2, unsigned int order) {
//     unsigned long pfn1 = page_to_pfn(page1);
//     unsigned long pfn2 = page_to_pfn(page2);
//     return (pfn1 ^ pfn2) == (1 << order) && (pfn1 & ((1 << order) - 1)) == 0;
// }
// 
// static inline 
// struct page *find_buddy_page(struct page *page, 
//                              unsigned long pfn, unsigned int order, unsigned long *buddy_pfn) {
//     unsigned long __buddy_pfn = find_buddy_pfn(pfn, order);
//     struct page *buddy = pfn_to_page(__buddy_pfn);
// 
//     buddy = page + (__buddy_pfn - pfn);
//     if (buddy_pfn) {
//         *buddy_pfn = __buddy_pfn;
//     }
// 
//     if (page_is_buddy(page, buddy, order)) {
//         return buddy;
//     }
//     return NULL;
// }

// static inline bool range_check_pfn(unsigned long pfn) {
//     if (pfn >= pfn_start && pfn < TOTAL_PAGES) {
//         //printf("error pfn: %d\n", pfn);
//         return true;
//     }
// 
//     return false;
// }

// 修改后的 get_buddy_page，接收显式 order 参数
static struct page *get_buddy_page(struct page *page, int order) {
    unsigned long page_pfn = page_to_pfn(page);
    uintptr_t page_addr = (uintptr_t)pfn_to_ptr(page_pfn);
    uintptr_t buddy_page_addr = page_addr ^ (1UL << (order + 12));
    unsigned long buddy_pfn = ptr_to_pfn((void *)buddy_page_addr);
    return pfn_to_page(buddy_pfn);
}

// static struct page *get_buddy_page(struct page *page) {
//     uintptr_t page_addr;
//     uintptr_t buddy_page_addr;
//     int order = page->order;
// 
//     unsigned long page_pfn = (uintptr_t)page_to_pfn(page);
//     printf("page_pfn: %d\n", page_pfn);
//     //panic_on(!range_check_pfn(page_pfn), "pfn error");
// 
//     page_addr = (uintptr_t)pfn_to_ptr(page_pfn);
//     printf("page_addr: 0x%x\n", page_addr);
//     //panic_on(!range_check_addr((void *)page_addr), "page_addr error");
// 
//     buddy_page_addr = page_addr ^ (1UL << (order + 12));
//     printf("buddy_addr: 0x%x\n", buddy_page_addr);
//     //panic_on(!range_check_addr((void *)buddy_page_addr), "buddy_page_addr error");
// 
//     unsigned long buddy_pfn = ptr_to_pfn((void *)buddy_page_addr);
//     //panic_on(!range_check_pfn(buddy_pfn), "pfn error");
// 
//     return pfn_to_page(buddy_pfn);
// }


static void split_block(struct page *block_page,
                 int *current_order,
                 int target_order) {
    panic_on(block_page == NULL, "block_page should not NULL");

    unsigned long block_pfn = page_to_pfn(block_page);
    debug_pf("block_page: 0x%x  block_pfn: %d\n", block_page, block_pfn);
    debug_pf("*current_order: %d  target_order: %d\n", *current_order, target_order);
    panic_on(block_pfn < pfn_start, "pfn error");

    while (*current_order > target_order) {
        (*current_order)--;
        struct page *buddy_page = get_buddy_page(block_page, *current_order);
        if (buddy_page == NULL) panic("buddy_page must be exist");

        buddy_page->order = *current_order;
        buddy_page->used = false;
        //buddy_page->compound_head = buddy_page->compound_head;

        //struct free_area *area = &free_lists[*current_order];
        //spin_lock(&area->lock);
        add_buddy_to_freelist(buddy_page, *current_order);
        //spin_unlock(&area->lock);

        block_page->order = *current_order;
    }
}


void *buddy_alloc(size_t size) {
    panic_on(size < PAGESIZE, "size must be at least PAGESIZE");

    int order = get_order(size);
    debug_pf("Requested size: 0x%x, required order: %d\n", size, order);

    int current_order = 0;
    for (current_order = order; current_order < MAX_ORDER; current_order++) {
        struct page *block_page = allocate_block(current_order);
        if (!block_page) continue;

        split_block(block_page, &current_order, order);

        void *return_addr = (void *)((uintptr_t)heap.start + 
                                     PAGESIZE * page_to_pfn(block_page));
        debug_pf("return block addr: 0x%x\n", return_addr);
        panic_on((uintptr_t)return_addr < start_used, 
                 "addr should not less than start_used");
        
        return return_addr;
    }

    debug_pf("No available blocks for order %d current_order: %d\n", 
             order, current_order);
    return NULL;
}

//------------free------------
static struct page *merge_pages(struct page *current_page, int current_order) {
    struct free_area *area = &free_lists[current_order];

    if (current_page->order == (MAX_ORDER - 1)) return current_page;

    //spin_lock(&area->lock);

    struct page *buddy_page = get_buddy_page(current_page, current_order);

    if (buddy_page == NULL || buddy_page->used == true
        || buddy_page->order != current_page->order) {
        goto done;
    }

    // panic_on(buddy_pfn < pfn_start, "pfn error");
    // panic_on(buddy_pfn >= TOTAL_PAGES, "pfn should not exceed TOTAL_PAGES");

    // if (buddy_page->used || buddy_page->order != current_order || 
    //     list_empty(&buddy_page->buddy_list)) {
    //     spin_unlock(&area->lock);
    //     return current_page;
    // }

    //if(free_lists[current_order].nr_free <= 1) goto done;

    remove_from_free_list(area, buddy_page);
    //spin_unlock(&area->lock);    
    
    buddy_page->order   += 1;
    current_page->order += 1;

    // choose main Block
    if (current_page > buddy_page) current_page = buddy_page;

    return merge_pages(current_page, current_page->order);

done:
    //spin_unlock(&area->lock);
    return current_page;;
}

// 功能：释放由伙伴系统分配的内存
// 先merge，再添加
void buddy_free(void *ptr) {
    if (ptr == NULL) return;

    unsigned long pfn = ptr_to_pfn(ptr);
    debug_pf("freeing ptr: 0x%x  freeing pfn: %d\n", ptr, pfn);
    panic_on(pfn >= TOTAL_PAGES, "pfn should not exceed TOTAL_PAGES");

    struct page *page = pfn_to_page(pfn);
    debug_pf("in free get page: %d\n", page_to_pfn(page));
    debug_pf("order: %d, used: %d\n", page->order, page->used);

    panic_on(page->used == false, "should not free unused memory.");
    page->used = false;

    struct page *current_page = page;
    //struct page *current_page = (page->compound_head) ? page->compound_head : page;
    //current_page->used = false;

    int order = current_page->order;
    current_page = merge_pages(current_page, order);

    //struct free_area *area = &free_lists[current_page->order];
    //spin_lock(&area->lock);
    add_buddy_to_freelist(current_page, current_page->order);
    //spin_unlock(&area->lock);
}


