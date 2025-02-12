#include <stddef.h>
#include <stdint.h>

#include <common.h>
#include <buddy.h>

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

struct page *get_page_from_free_area(int order)
{
    debug_pf("\n\ninssssssssssssssssssssssssss nr_free: %d\n", free_lists[order].nr_free);
    debug_pf("head: 0x%x\n", free_lists[order].head);
    debug_pf("&head: 0x%x\n", &free_lists[order].head);
    debug_pf("head->prev: 0x%x\n", free_lists[order].head.prev);
    debug_pf("head->next: 0x%x\n", free_lists[order].head.next);

    debug_pf("page->next to page, and order is 0x%x\n",
             ((struct page *)(free_lists[order].head.next))->order);
    debug_pf("page->next to page, and buddy_list is 0x%x\n", 
             ((struct page *)(free_lists[order].head.next))->buddy_list);

	return list_first_entry_or_null(&free_lists[order].head,
					struct page, buddy_list);
}

static void *allocate_block(int order) {
    struct free_area *area = &free_lists[order];

    // print_free_lists_nr_free();

    struct page *block_page = get_page_from_free_area(order);
    // if (area->nr_free == 0) {
    //     return NULL;
    // }
    // if (block_page== NULL) {
    //     debug_pf("order is %d  block is NUL1111111111111\n", order);
    //     return NULL;
    // } else {
    //     debug_pf("block is not NULL........\n");
    // }

    if (block_page == NULL || area->nr_free == 0) return NULL; 

     //也就是说，明明还有空闲块（nr_free > 0）
     // 但是却被判断成了空的块？
     // if (list_empty(area->head) || area->nr_free == 0){
     //     // if (list_empty(area->head)) {
     //     //     debug_pf("dsao210321243eadc.......\n");
     //     // }
     //     // debug_pf("now order is %d, nr_free is %d\n", order, free_lists[order].nr_free);
     //     // panic_on(area->head->next != area->head->prev, "area is not empty");
     //     return NULL;
     // }

    // 另外：
    // block is not NULL........
    // area->nr_free: 0
    // 我理解的该阶没有了block，不久应该是NULL？
    // 也就是head->next = haed 成立，那就是empty

    // 获取链表中的第一个 page 块
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

// static void add_buddy_to_freelist(struct page *buddy_page, int order) {
//     struct free_area *area = &free_lists[order];
//     list_add(&buddy_page->buddy_list, area->head);
//     area->nr_free++;
// }

//     //这里的逻辑不太对？？
//     //block_node 指向的是要被删除的那个结点？
//     //然后之后的元数据赋值，处理的也是这个被删除的结点？
//     //ok
//     // 使用container_of宏正确获取page结构体指针
//     struct page *block_page = list_entry(block_node, struct page, buddy_list);

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

        int original_order = current_order;
        split_block(block_page, &current_order, order);

        debug_pf("Allocated block 0x%x at pfn %u (order %d->%d)\n",
               block_page, page_to_pfn(block_page), original_order, current_order);

        // 哦对！还要减掉 元数据用了多少页？
        // 那直接从heap.start开始？！
        debug_pf("start_used: 0x%x  pfn: %d\n", start_used, page_to_pfn(block_page));
        debug_pf("used_addr: 0x%x\n", start_used + PAGESIZE * page_to_pfn(block_page));
        //return (void *)(start_used + PAGESIZE * page_to_pfn(block_page));
        return (void *)((uintptr_t)heap.start+ PAGESIZE * page_to_pfn(block_page));
    }

    debug_pf("No available blocks for order %d current_order: %d\n", order, current_order);
    return NULL;
}
