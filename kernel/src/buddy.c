#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <os/common.h>
#include <os/buddy.h>
#include <os/list.h>

// TODO: should use is_slab after starting slab...
#ifdef  DEBUG
void print_free_lists_nr_free() {
    for (int i = MAX_ORDER - 1; i >= 0; i--) {
        printf("free_lists[%d].nr_free: %d\n",
                 i, free_lists[i].nr_free);
    }
    printf("=======================\n");
}
#else 
void print_free_lists_nr_free() {
}
#endif

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

    panic_on(block_page->used == true, "block in free_area should not be used");
    panic_on(area->nr_free == 0, "free_area should have block");
    panic_on(ptr < start_used, "addr should not less than start_used");
    //panic_on(block_page->padding != MAGIC, "MAGIC error");

    remove_from_free_list(area, block_page);

    // printf("current_alloc_block_page: %d\n", page_to_pfn(block_page));
    block_page->used = true;
    block_page->order = order;

    return block_page;
}

static void add_buddy_to_freelist(struct page *buddy_page, int order) {
    struct free_area *area = &free_lists[order];
    list_add(&buddy_page->buddy_list, (struct list_head *)&free_lists[order]);
    area->nr_free++;
    debug_pf("in list_add  now buddy_page: 0x%x\n", buddy_page);
}


// static inline unsigned long find_buddy_pfn(unsigned long page_pfn, unsigned int order) {
//     //debug_pf("page_pfn: 0x%x  order: %d\n", page_pfn, order);
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
//     struct page *buddy = pfn_to_page(__buddy_pfn);  // 直接使用 pfn_to_page
// 
//     if (buddy_pfn) {
//         *buddy_pfn = __buddy_pfn;
//     }
// 
//     if (page_is_buddy(page, buddy, order)) {
//         return buddy;
//     }
//     return NULL;
// }

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

//#define PAGE_VERSION
#ifdef PAGE_VERSION
static void split_block(struct page *block_page, int *current_order, int target_order) {
    unsigned long block_pfn = page_to_pfn(block_page);
    debug_pf("block_page: 0x%x  block_pfn: %d\n", block_page, block_pfn);
    debug_pf("*current_order: %d  target_order: %d\n", *current_order, target_order);
    panic_on(block_pfn < pfn_start, "pfn error");

    unsigned long buddy_pfn = 0;

    while (*current_order > target_order) {
        (*current_order)--;
        debug_pf("now order: %d\n", *current_order);

       // 检查当前块的 PFN 是否对齐到 current_order
        panic_on((block_pfn & ((1UL << *current_order) - 1)) != 0,
                 "Block PFN not aligned to current_order");

        // Check if the buddy page is valid
        struct page *buddy_page = find_buddy_page(block_page, block_pfn, *current_order, &buddy_pfn);
        if (!buddy_page) {
            //panic("Invalid buddy page found");
            debug_pf("Warning: Invalid buddy page found. Skipping split.\n");
            //goto done;
            continue;
        }

        debug_pf("buddy_pfn: %u  buddy_page: 0x%x\n", buddy_pfn, buddy_page);
        panic_on(buddy_pfn < pfn_start || buddy_pfn >= 0x7d00, "Invalid buddy PFN");
        debug_pf("buddy_pfn: %d\n", buddy_pfn);

        // 更新伙伴块元数据
        buddy_page->order = *current_order;
        buddy_page->used = false;
        buddy_page->compound_head = buddy_page;

        add_buddy_to_freelist(buddy_page, *current_order);

//done:
        // 更新当前块元数据
        block_page->order = *current_order;
        block_pfn = page_to_pfn(block_page);
    }
}

#else 
static void split_block(struct page *block_page,
                 int *current_order,
                 int target_order) {
    panic_on(block_page == NULL, "block_page should not NULL");

    // debug_pf("block_page->padding: 0x%x line: %d\n", block_page->padding, __LINE__);
    // panic_on(block_page->padding != MAGIC, "MAGIC error");

    unsigned long block_pfn = page_to_pfn(block_page);
    debug_pf("block_page: 0x%x  block_pfn: %d\n", block_page, block_pfn);
    debug_pf("*current_order: %d  target_order: %d\n", 
             *current_order, target_order);
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

        // unsigned long buddy_pfn = find_buddy_pfn(block_pfn, *current_order);
        // struct page *buddy_page = pfn_to_page(buddy_pfn);
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

        add_buddy_to_freelist(buddy_page, *current_order);
        
        // 更新当前块元数据（order会在循环最后更新）
        block_page->order = *current_order;
    }
}
#endif

void *buddy_alloc(size_t size) {
    panic_on(size < PAGESIZE, "size must be at least PAGESIZE");

    int order = get_order(size);
    debug_pf("Requested size: 0x%x, required order: %d\n", size, order);

    int current_order = 0;
    for (current_order = order; current_order < MAX_ORDER; current_order++) {
        struct page *block_page = allocate_block(current_order);
        if (!block_page) continue;
        // debug_pf("block_page->padding: 0x%x line: %d\n",
        //          block_page->padding, __LINE__);
        // panic_on(block_page->padding != MAGIC, "MAGIC error");

        split_block(block_page, &current_order, order);

#ifdef DEBUG
        int original_order = current_order;
        debug_pf("Allocated block 0x%x at pfn %u (order %d->%d)\n",
               block_page, page_to_pfn(block_page), 
               original_order, current_order);
#endif

        void *return_addr = (void *)((uintptr_t)heap.start + 
                                     PAGESIZE * page_to_pfn(block_page));
        debug_pf("return block addr: 0x%x\n", return_addr);
        panic_on((uintptr_t)return_addr < start_used, 
                 "addr should not less than start_used");

        // printf("return addr: 0x%x pfn: %d  now order: %d\n",
        //        return_addr, ptr_to_pfn(return_addr), current_order);
        return return_addr;
    }

    debug_pf("No available blocks for order %d current_order: %d\n", 
             order, current_order);
    return NULL;
}

//------------free------------
static struct page *merge_pages(struct page *current_page, int current_order) {
    if (current_page->order == (MAX_ORDER - 1)) return NULL;
    debug_pf("enter order: %d\n", current_order);
    
    unsigned long current_pfn = page_to_pfn(current_page);
    //unsigned long buddy_pfn = 0;

    uintptr_t current_addr = (uintptr_t)pfn_to_ptr(current_pfn);
    uintptr_t buddy_addr = current_addr ^ (1UL << (current_order + 12));
    unsigned long buddy_pfn = ptr_to_pfn((void *)buddy_addr);
    struct page *buddy_page = pfn_to_page(buddy_pfn);


    // struct page *buddy_page = find_buddy_page(current_page, current_pfn, 
    //                                          current_order, &buddy_pfn);
    // printf("buddy_page: 0x%x  current_page: 0x%x\n", buddy_page, current_page);

    if (buddy_page == NULL) return current_page;

    if (buddy_page->used == true) return current_page;

    if (buddy_page->order != current_page->order) return current_page;

    if(free_lists[current_order].nr_free <= 1) return current_page;

    struct free_area *area = &free_lists[current_order];
    remove_from_free_list(area, buddy_page);
    
    buddy_page->order   += 1;
    current_page->order += 1;
    // printf("after upadte, now order: %d\n", buddy_page->order);
    // printf("in merge print.................-.............\n");
    print_free_lists_nr_free();

    // choose main Block
    if (current_page > buddy_page) {
        current_page = buddy_page;
    }
    
    return merge_pages(current_page, current_page->order);
}

// 功能：释放由伙伴系统分配的内存
// 先merge，再添加
void buddy_free(void *ptr) {
    if (ptr == NULL) return;

    unsigned long pfn = ptr_to_pfn(ptr);
    debug_pf("freeing ptr: 0x%x  freeing pfn: %d\n", ptr, pfn);
    panic_on(pfn >= TOTAL_PAGES, "pfn should not exceed TOTAL_PAGES");

    // 获取对应的 struct page
    struct page *page = pfn_to_page(pfn);
    debug_pf("in freem get page: %d\n", page_to_pfn(page));
    debug_pf("order: %d, used: %d\n", page->order, page->used);

    panic_on(page->used == false, "should not free unused memory.");
    page->used = false;

    struct page *current_page = page;
    //struct page *current_page = (page->compound_head) ? page->compound_head : page;
    //current_page->used = false;

    int order = current_page->order;
    current_page = merge_pages(current_page, order);

    int new_order = current_page->order;

    add_buddy_to_freelist(current_page, new_order);
}


