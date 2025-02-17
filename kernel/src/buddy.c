#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <common.h>
#include <buddy.h>
#include "list.h"

// TODO: should use is_slab after starting slab...
void print_free_lists_nr_free() {
    for (int i = MAX_ORDER - 1; i >= 0; i--) {
        debug_pf("free_lists[%d].nr_free: %d\n",
                 i, free_lists[i].nr_free);
    }
    debug_pf("=======================\n\n");
}

static void remove_from_free_list(struct free_area *area, struct page *page) {
    panic_on(area->nr_free == 0, "should not remove empty free_lists");
    list_del(&page->buddy_list);
    area->nr_free--;
}

// struct page *get_page_from_free_area(int order) {
//     struct free_area *area = &free_lists[order];
//     struct list_head *head = &area->head;
// 
//     if (list_empty(head)) {
//         debug_pf("free_lists[%d] is empty\n", order);
//         return NULL;
//     }
// 
//     struct page *block_page = list_first_entry(head, struct page, buddy_list);
//     return block_page;
// }

struct page *get_page_from_free_area(int order) {

#ifdef DEBUG
    // 被覆写了？？
    debug_pf("\nnowxxxxxxxxxxxxfree_lists[%d].nr_free: %d\n", order, free_lists[order].nr_free);
    debug_pf("head: 0x%x\n", free_lists[order].head);
    debug_pf("&head: 0x%x\n", &free_lists[order].head);
    debug_pf("head->prev: 0x%x\n", free_lists[order].head.prev);
    debug_pf("head->next: 0x%x\n", free_lists[order].head.next);

    struct page *page_next = list_entry(free_lists[order].head.next, struct page, buddy_list);
    debug_pf("page_next addr; 0x%x\n", page_next);
    debug_pf("page->next to page, and order is 0x%x\n", page_next->order);

    debug_pf("page_next->order: %d and addr: 0x%x\n",
             page_next->order, &page_next->order);

    debug_pf("page_next->buddy_list: 0x%x\n", 
             page_next->buddy_list);
#endif

    // debug_pf("page_next->padding addr: 0x%x\n", &page_next->padding);
    // debug_pf("page_next->padding: 0x%x  line: %d\n", page_next->padding, __LINE__);
    // panic_on(page_next->padding != MAGIC, "MAGIC error");

    // debug_pf("page->next to page, and order is 0x%x\n",
    //          ((struct page *)(free_lists[order].head.next))->order);
    // debug_pf("page->next to page, and &order is 0x%x\n",
    //          &(((struct page *)(free_lists[order].head.next))->order));

    // debug_pf("page->next to page, and buddy_list is 0x%x\n", 
    //          ((struct page *)(free_lists[order].head.next))->buddy_list);

    // debug_pf("page->next to page, and buddy_list is 0x%x\n", 
    //          &(((struct page *)(free_lists[order].head.next))->buddy_list));

	return list_first_entry_or_null(&free_lists[order].head,
					struct page, buddy_list);
}

static void *allocate_block(int order) {
    struct free_area *area = &free_lists[order];
    
    print_free_lists_nr_free();
    
    if (list_empty(&area->head) || area->nr_free == 0) return NULL;

    struct page *block_page = get_page_from_free_area(order);
    if (block_page == NULL) return NULL;


    unsigned long pfn = page_to_pfn(block_page);
    uintptr_t ptr = (uintptr_t)pfn_to_ptr(pfn);
    debug_pf("block_page: 0x%x\n", block_page);
    debug_pf("block_pfn: %d\n", pfn);
    debug_pf("block_ptr: 0x%x\n", ptr);
    panic_on(pfn < pfn_start, "pfn error");

    //if (block_page == NULL || area->nr_free == 0) return NULL;
    // if (list_empty(&area->head)) return NULL;
    // || block_page->used == true || ptr < start_used) return NULL;
    // 既然在free_lists中取出来的，那就不应该是used。
    // 另外，该阶的 nr_free 也必须不为0

    panic_on(block_page->used == true, "block in free_area should not be used");
    panic_on(area->nr_free == 0, "free_area should have block");
    panic_on(ptr < start_used, "addr should not less than start_used");
    //panic_on(block_page->padding != MAGIC, "MAGIC error");

    //if (block_page->is_slab == true) return NULL;

    remove_from_free_list(area, block_page); // 传入要删除的块指针

    block_page->used = true;
    block_page->order = order;

    return block_page;
}

static void add_buddy_to_freelist(struct page *buddy_page, int order) {
    struct free_area *area = &free_lists[order];
    list_add(&buddy_page->buddy_list, (struct list_head *)&free_lists[order]);
    area->nr_free++; // 增加空闲块的数量

    debug_pf("in list_add  now buddy_page: 0x%x\n", buddy_page);
}

// static inline unsigned long find_buddy_pfn(unsigned long page_pfn, unsigned int order) {
//     debug_pf("page_pfn: 0x%x  order: %d\n", page_pfn, order);
//     return page_pfn ^ (1 << order);
//     // return (page_pfn ^ (1 << order)) + pfn_start;
// }

static void split_block(struct page *block_page,
                 int *current_order,
                 int target_order) {
    // panic_on(block_page == NULL, "block_page should not NULL");
    // debug_pf("block_page->padding: 0x%x line: %d\n", block_page->padding, __LINE__);
    // panic_on(block_page->padding != MAGIC, "MAGIC error");

    unsigned long block_pfn = page_to_pfn(block_page);
    debug_pf("block_page: 0x%x  block_pfn: %d\n", block_page, block_pfn);
    debug_pf("*current_order: %d  target_order: %d\n", *current_order, target_order);
    panic_on(block_pfn < pfn_start, "pfn error");

    uintptr_t block_addr = (uintptr_t)pfn_to_ptr(block_pfn);
    debug_pf("now block_addr: 0x%x\n", block_addr);

    while (*current_order > target_order) {
        (*current_order)--;
        
         // 检查当前块是否对齐到当前order
        //  panic_on(block_pfn & ((1UL << *current_order) - 1),
        //           "block_pfn pfn not aligned to current_order");
        //  debug_pf("Block PFN 0x%lx not aligned to order %d",block_pfn, *current_order);

        // 计算伙伴块的PFN（异或操作是伙伴系统的核心）
        //unsigned long buddy_pfn = block_pfn ^ (1UL << *current_order);
        
        uintptr_t buddy_addr = block_addr ^ (1UL << (*current_order + 12));
        debug_pf("now buddy_addr: 0x%x\n", buddy_addr);

        unsigned long buddy_pfn = ptr_to_pfn((void *)buddy_addr);
        struct page *buddy_page = pfn_to_page(buddy_pfn);
        

        //unsigned long buddy_pfn = find_buddy_pfn(block_pfn, *current_order);
        //struct page *buddy_page = pfn_to_page(buddy_pfn);
        // debug_pf("buddy_page->padding: 0x%x  line: %d\n", buddy_page->padding, __LINE__);
        // panic_on(buddy_page->padding != MAGIC, "MAGIC error");

        debug_pf("now order: %d\n", *current_order);
        debug_pf("buddy_pfn: %u  buddy_page: 0x%x\n", buddy_pfn, buddy_page);
        if (buddy_pfn < pfn_start || buddy_pfn >= 0x7d00) {
            // 处理无效伙伴块，或触发错误
            panic("Invalid buddy PFN for current_order");
        }

        buddy_page->order = *current_order;
        buddy_page->used = false;
        buddy_page->compound_head = buddy_page->compound_head;
        //buddy_page->compound_head = buddy_page;

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
        //debug_pf("block_page->padding: 0x%x line: %d\n", block_page->padding, __LINE__);
        //panic_on(block_page->padding != MAGIC, "MAGIC error");

        //int original_order = current_order;
        split_block(block_page, &current_order, order);

        // debug_pf("Allocated block 0x%x at pfn %u (order %d->%d)\n",
        //        block_page, page_to_pfn(block_page), original_order, current_order);

        void *return_addr = (void *)((uintptr_t)heap.start+ PAGESIZE * page_to_pfn(block_page));
        debug_pf("start_used: 0x%x  pfn: %d\n", 
                 start_used, page_to_pfn(block_page));
        debug_pf("return block addr: 0x%x\n", return_addr);
        panic_on((uintptr_t)return_addr < start_used, 
                 "addr should not less than start_used");

        //return (void *)((uintptr_t)heap.start+ PAGESIZE * page_to_pfn(block_page));
        return return_addr;
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
    debug_pf("freeing pfn: %d\n", pfn);
    debug_pf("Allocated: 0x%x  freeing pfn: %d\n", ptr, pfn);
    panic_on(pfn >= TOTAL_PAGES, "pfn should not exceed TOTAL_PAGES");

    struct page *page = pfn_to_page(pfn);
    printf("page addr: 0x%x\n", page);
    printf("buddy_list: 0x%x compound_head: 0x%x\n", page->buddy_list, page->compound_head);
    printf("order: %d is_slab: %d use: %d\n", page->order, page->is_slab, page->used);

    panic_on(page->used == false, "should not free unused memory.\n");
    panic_on(page->is_slab == true, "slab should not be true");
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

