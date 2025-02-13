#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <common.h>
#include <buddy.h>

// TODO: should use is_slab after starting slab...
void print_free_lists_nr_free() {
    for (int i = MAX_ORDER - 1; i >= 0; i--) {
        debug_pf("free_lists[%d].nr_free: %d\n",
                 i, free_lists[i].nr_free);
    }
}

static void remove_from_free_list(struct free_area *area, struct page *page) {
    list_del(&page->buddy_list);
    area->nr_free--;
}

struct page *get_page_from_free_area(int order) {
    // debug_pf("\n\ninssssssssssssssssssssssssss nr_free: %d\n", free_lists[order].nr_free);
    // debug_pf("head: 0x%x\n", free_lists[order].head);
    // debug_pf("&head: 0x%x\n", &free_lists[order].head);
    // debug_pf("head->prev: 0x%x\n", free_lists[order].head.prev);
    // debug_pf("head->next: 0x%x\n", free_lists[order].head.next);

    // debug_pf("page->next to page, and order is 0x%x\n",
    //          ((struct page *)(free_lists[order].head.next))->order);
    // debug_pf("page->next to page, and buddy_list is 0x%x\n", 
    //          ((struct page *)(free_lists[order].head.next))->buddy_list);

	return list_first_entry_or_null(&free_lists[order].head,
					struct page, buddy_list);
}

static void *allocate_block(int order) {
    struct free_area *area = &free_lists[order];

    // print_free_lists_nr_free();

    struct page *block_page = get_page_from_free_area(order);
    if (block_page == NULL || area->nr_free == 0) return NULL; 

    //if (block_page->is_slab == true) return NULL;

    remove_from_free_list(area, block_page); // 传入要删除的块指针

    block_page->used = true;
    block_page->order = order;

    return block_page;
}

static void add_buddy_to_freelist(struct page *buddy_page, int order) {
    struct free_area *area = &free_lists[order];
    list_add(&buddy_page->buddy_list, &free_lists[order].head);
    area->nr_free++; // 增加空闲块的数量
}

static void split_block(struct page *block_page,
                 int *current_order,
                 int target_order) {
    panic_on(block_page == NULL, "block_page should not NULL");

    unsigned long block_pfn = page_to_pfn(block_page);
    debug_pf("block_page: 0x%x  block_pfn: %u\n", block_page, block_pfn);
    debug_pf("*current_order: %d  target_order: %d\n", *current_order, target_order);

    while (*current_order > target_order) {
        //print_free_lists_nr_free();
        (*current_order)--;
        
        // 计算伙伴块的PFN（异或操作是伙伴系统的核心）
        unsigned long buddy_pfn = block_pfn ^ (1UL << *current_order);
        struct page *buddy_page = pfn_to_page(buddy_pfn);


        debug_pf("buddy_pfn: %u  buddy_page: 0x%x\n", buddy_pfn, buddy_page);

        buddy_page->compound_head = buddy_page;
        buddy_page->order = *current_order;
        buddy_page->used = false;

        add_buddy_to_freelist(buddy_page, *current_order);
        
        // 更新当前块元数据（order会在循环最后更新）
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

        //int original_order = current_order;
        split_block(block_page, &current_order, order);

        debug_pf("Allocated block 0x%x at pfn %u (order %d->%d)\n",
               block_page, page_to_pfn(block_page), original_order, current_order);

        debug_pf("start_used: 0x%x  pfn: %d\n", 
                 start_used, page_to_pfn(block_page));
        debug_pf("return block addr: 0x%x\n", 
                 (uintptr_t)heap.start + PAGESIZE * page_to_pfn(block_page));
        return (void *)((uintptr_t)heap.start+ PAGESIZE * page_to_pfn(block_page));
    }

    debug_pf("No available blocks for order %d current_order: %d\n", 
             order, current_order);
    return NULL;
}

//------------free------------
// static struct page *get_buddy_page(struct page *page, int order) { unsigned long page_pfn = page_to_pfn(page);
//     unsigned long buddy_pfn = page_pfn ^ (1UL << order);
//     panic_on(page_pfn >= TOTAL_PAGES, "page_pfn should not exceed TOTAL_PAGES");
//     return pfn_to_page(buddy_pfn);
// }

static void try_merge_buddies(struct page *page, int order) {
    struct page *current_page = page;
    while (order < MAX_ORDER - 1) {
        unsigned long current_pfn = page_to_pfn(current_page);
        unsigned long buddy_pfn = current_pfn ^ (1UL << order);
        struct page *buddy_page = pfn_to_page(buddy_pfn);

        debug_pf("current_pfn %d  buddy_pfn: %d\n", current_pfn, buddy_pfn);
        debug_pf("buddy: 0x%x\n", buddy_page);

        // 检查伙伴是否空闲、同阶且未被使用
        // 伙伴块是否是头
        if (!buddy_page || buddy_page->used || buddy_page->order != order
            || (buddy_page->compound_head != current_page)
            || (current_page->compound_head != current_page)) break;

        struct page *main = current_pfn < buddy_pfn ? 
                                current_page : buddy_page;
        struct page *secondary = current_pfn < buddy_pfn ? 
                                buddy_page : current_page;

        // 确定合并后的头块（取PFN较小的）
        remove_from_free_list(&free_lists[order], main);
        remove_from_free_list(&free_lists[order], secondary);

        main->order = order + 1;
        main->compound_head = main;
        main->used = false;
        secondary->compound_head = main;

        // 将合并后的块作为新基准，继续尝试合并
        current_page = main;
        order++;
    }

    debug_pf("now order: %d\n", order);
    current_page->order = order;

    // 将最终合并的块加入空闲链表
    add_buddy_to_freelist(current_page, order);
}

void buddy_free(void *ptr) {
    panic_on(ptr == NULL, "should not free NULL ptr\n");

    unsigned long pfn = ptr_to_pfn(ptr);
    debug_pf("freeomg pfn: %d\n", pfn);
    panic_on(pfn >= TOTAL_PAGES, "pfn should not exceed TOTAL_PAGES");

    struct page *page = pfn_to_page(pfn);
    panic_on(page->used == false, "should not free unused memory.\n");
    panic_on(page->is_slab == true, "should not use slab");
    panic_on(page->order >= MAX_ORDER, "InvalIid order when freeing");
    if (page->order != get_order(PAGESIZE * (1 << page->order))) {
        panic("Order mismatch detected during free!");
    }

    struct page *current_page = (page->compound_head) ? 
                                page->compound_head : page;
    // 标记为未使用并获取原始阶数
    current_page->used = false;

    int order = current_page->order;

    // 尝试合并伙伴块
    try_merge_buddies(current_page, order);
}

