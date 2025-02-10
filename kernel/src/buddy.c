#include <stddef.h>
#include <common.h>

#include <buddy.h>
// 打印 free_lists 各阶链表中的 free_area
void print_free_areas(void) {
    int order;
    struct free_area *area;
    // struct page *page;

    for (order = 0; order < MAX_ORDER; order++) {
        area = &free_lists[order];

        // 检查链表是否为空
        if (list_empty(area->head)) {
            debug_pf("Order %d: empty\n", order);
            continue;
        }

        // 打印 nr_free 和链表中的 page 地址
        debug_pf("Order %d: nr_free = %u\n", order, area->nr_free);

        // list_for_each_entry(page, area->head, buddy_list) {
        //     debug_pf("    Page address: %p\n", page);
        // }
    }
}

static void remove_from_free_list(struct free_area *area) {
    if (area->nr_free == 0) return;
    struct list_head *block_node = area->head;
    area->head = block_node->next;
    list_del(block_node);
    area->nr_free--;
}

// static unsigned long set_block_meta_data() {
// }

// static unsigned long set_block_meta_data() {
// }

static void *allocate_block(int order) {
    struct free_area *area = &free_lists[order];

    if (area->nr_free == 0) return NULL;
    panic_on(area->nr_free == 0, "nr_free should > 0");

    remove_from_free_list(area);

    struct page *block_page = (struct page *)area->head;
    block_page->used = true;
    block_page->order = order;

    return block_page;
}

// static void *allocate_block(int order) {
//     struct free_area *area = &free_lists[order];
//     if (area->nr_free == 0) return NULL;
// 
//     print_free_areas();
// 
//     struct list_head *block_node = area->head;
//     remove_from_free_list(area);
// 
//     // 使用container_of宏正确获取page结构体指针
//     struct page *block_page = list_entry(block_node, struct page, buddy_list);
//     block_page->used = true;
//     block_page->order = order;
// 
//     return block_page;
// }
// 

// 当链表为空时，初始化头节点并将伙伴块添加为唯一节点；
// 否则将新块添加到链表头部，保证分配时优先使用最近分裂的块。
static void add_buddy_to_freelist(struct page *buddy_page, int order) {
    struct free_area *area = &free_lists[order];
    if (area->head == NULL) {
        area->head = &buddy_page->buddy_list;
        INIT_LIST_HEAD(area->head);
    } else {
        list_add(&buddy_page->buddy_list, area->head);  
    }
    area->nr_free++;
}

static void split_block(struct page *block_page,
                 int *current_order,
                 int target_order) {
    unsigned long block_pfn = page_to_pfn(block_page);
    while (*current_order > target_order) {
        (*current_order)--;
        debug_pf("in split_block, current_order: %d, target_order: %d\n",
                 *current_order, target_order);
        
        // 计算伙伴块的PFN（异或操作是伙伴系统的核心）
        unsigned long buddy_pfn = block_pfn ^ (1UL << *current_order);
        struct page *buddy_page = pfn_to_page(buddy_pfn);

        debug_pf("block_page: 0x%x block_pfn: %u\n", block_page, block_pfn);
        debug_pf("buddy_pfn: %u  buddy_page: 0x%x\n", buddy_pfn, buddy_page);

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

    for (int current_order = order; current_order < MAX_ORDER; current_order++) {
        struct page *block_page = allocate_block(current_order);
        if (!block_page) continue;

        int original_order = current_order;
        split_block(block_page, &current_order, order);

        debug_pf("Allocated block at pfn %u (order %d->%d)\n",
               page_to_pfn(block_page), original_order, current_order);

        debug_pf("block_page: 0x%x to pfn: %d\n", block_page, page_to_pfn(block_page));

        return (void *)(heap.start + page_to_pfn(block_page) * PAGESIZE);
    }

    debug_pf("No available blocks for order %d\n", order);
    return NULL;
}
