#include <buddy.h>
#include <common.h>

// void debug_free_list(struct free_area *free_lists, int order) {
//     struct page *page;
//     struct list_head *head = free_lists[order].head; // 链表头指针
//     struct list_head *current;                         // 当前节点指针
// 
//     debug_pf("free_lists[%d].nr_free = %d\n", order, free_lists[order].nr_free);
// 
//     // 获取链表头的下一个节点
//     current = head->next;
// 
//     // 开始遍历链表
//     while (1) {
//         // 计算当前节点对应的 struct page 的地址
//         page = (struct page *)((char *)current);
// 
//         // 打印当前页面的信息
//         debug_pf("Page in order %d: 0x%x (compound_head: 0x%x)\n",
//                  order, page, (uintptr_t)page->compound_head);
// 
//         // 获取下一个链表节点
//         current = current->next;
// 
//         // 如果遍历到链表头节点，退出循环
//         if (current == head) {
//             break;
//         }
//     }
// }

static void init_free_block(struct free_area *free_lists,
                     unsigned long start_pfn, unsigned long end_pfn) {
    if (start_pfn > end_pfn) panic("Invalid PFN range");
    unsigned long remaining_pages = end_pfn - start_pfn + 1;

    for (int order = MAX_ORDER - 1; order >= 0; order--) {
        unsigned long block_size = 1UL << order;

        // 分配块的头页PFN（从后向前）
        while (remaining_pages >= block_size) {
            unsigned long head_pfn = start_pfn + remaining_pages - block_size;
            struct page *head_page = pfn_to_page(head_pfn);

            debug_pf("head_pfn: 0x%x  head_page: 0x%x\n",
                     head_pfn, head_page);

            head_page->used = false;
            head_page->order = order;
            head_page->compound_head = head_page;  // 头页指向自身
            
            debug_pf("head_page->compound_head: 0x%x\n", head_page->compound_head);

            // 问题应该在这里，free_lists->head_page->buddy_list

            // 初始化尾页元数据
            for (unsigned long pfn = head_pfn + 1; pfn < head_pfn + block_size; pfn++) {
                struct page *tail_page = pfn_to_page(pfn);
                tail_page->used = false;
                tail_page->order = -1;             // 标记为尾页
                tail_page->compound_head = head_page; // 尾页指向头页
                INIT_LIST_HEAD(&tail_page->buddy_list);
                list_add_tail(&tail_page->buddy_list, &head_page->buddy_list);
                //list_add(&tail_page->buddy_list, &head_page->buddy_list);
            }

            // 将头页的buddy_list加入free_lists[order].head
            
            // 这里搞晕了，一直不明白为什么head->next一直都不变
            // 其实用的list_add_tail，插到尾部，肯定头不变啊
            // 不过再想想，因为我是从后向前分配页的，
            // 加上自己想低阶/各阶首个空闲块地址是从小到大
            // 应该用list_add

            // 关键的这一句，一直没想明白是怎么回事
            // 别忘了free_lists[order].head这个也是一个struct page
                
            if (free_lists[order].head == NULL) {
                debug_pf("now order: %d\n", order);
                free_lists[order].head = (struct list_head *)(&(head_page->buddy_list)); 
                INIT_LIST_HEAD(free_lists[order].head);
            } else {
                debug_pf("now order: %d head_page: 0x%x\n", order, head_page);
                list_add_tail(&head_page->buddy_list, free_lists[order].head);
            }
            free_lists[order].nr_free++;

            remaining_pages -= block_size;

            // TODO 学习，linux遍历链表的API
            // struct page *current_page;
            // list_for_each_entry(current_page, free_lists[order].head, buddy_list) {
            //     debug_pf("Node in free_lists[%d]: 0x%x free_lists[%d].nr_free: %d\n", 
            //              order, current_page, order, free_lists[order].nr_free);
            // }

            unsigned long pfn = page_to_pfn(head_page);
            debug_pf("free_lists[%d].nr_free: %d  free_list[%d].head: 0x%x  "
                     "free_lists[%d].head: next=0x%x prev=0x%x  pfn=%d\n", 
                     order, free_lists[order].nr_free,
                     order, free_lists[order].head,
                     order, free_lists[order].head->next, 
                            free_lists[order].head->prev, pfn);

            debug_pf("now order: %d  start_pfn: 0x%x  end_pfn: 0x%x(%d)  "
                     "remaining_pages: 0x%x  block_size: 0x%x(%d)\n\n",
                     order, start_pfn, end_pfn, end_pfn,
                     remaining_pages, block_size, block_size);
        }
            debug_pf("free_lists[%d].nr_free: %d  free_list[%d].head: 0x%x  "
                     "free_lists[%d].head: next=0x%x prev=0x%x  end current_order\n", 
                     order, free_lists[order].nr_free,
                     order, free_lists[order].head,
                     order, free_lists[order].head->next, 
                            free_lists[order].head->prev);
    }

    for (int i = MAX_ORDER - 1; i >= 0; i--) {
        debug_pf("free_lists[%d].nr_free: 0x%x(%d)\n",
                 i, free_lists[i].nr_free, free_lists[i].nr_free);
    }
}

static size_t init_page_meta_data(unsigned long *start_pfn, unsigned long *end_pfn) {
    uintptr_t total_pages = HEAP_SIZE / PAGESIZE;
    debug_pf("total_pages: 0x%x\n", total_pages);
    debug_pf("start_pfn: 0x%x  end_pfn: 0x%x(%d)\n", *start_pfn, *end_pfn, *end_pfn);

    // 计算元数据区大小及所占页数
    size_t pagedata_size = total_pages * sizeof(struct page);
    unsigned long pagedata_pages = (pagedata_size + PAGESIZE - 1) / PAGESIZE;

    struct page *page_meta = (struct page *)heap.start;
    for (size_t pfn = 0; pfn < total_pages; pfn++) {
        page_meta[pfn].used = (pfn < pagedata_pages);
        page_meta[pfn].is_slab = false;
        page_meta[pfn].order = 0;
        INIT_LIST_HEAD(&page_meta[pfn].buddy_list);
    }

    debug_pf("metadata_size: 0x%x  metadata_pages: 0x%x(%d)\n", pagedata_size, pagedata_pages, pagedata_pages);
    (*start_pfn) += pagedata_pages;
    debug_pf("start_pfn: 0x%x  end_pfn: 0x%x(%d)\n", *start_pfn, *end_pfn, *end_pfn);
    debug_pf("==============\n");

    return pagedata_size;
}

static struct free_area *init_free_lists(unsigned long *start_pfn, 
                     unsigned long *end_pfn,
                     struct free_area *free_list_addr) {
    struct free_area *free_lists_tmp = (struct free_area *)free_list_addr;

    // for (int i = MAX_ORDER - 1; i >= 0; i--) {
    //     debug_pf("free_lists[%d].nr_free: 0x%x(%d)  ",
    //              i, free_lists[i].nr_free, free_lists[i].nr_free);
    //     debug_pf("now free_lists addr: 0x%x\n", &free_lists[i]);
    // }
    // 静态：LIST_HEAD_INIT
    // 动态：INIT_LIST_HEAD
    for (int i = 0; i < MAX_ORDER; i++) {
        INIT_LIST_HEAD(free_lists_tmp[i].head);
        free_lists_tmp[i].nr_free = 0;

        debug_pf("====sizoeof(free_lists): 0x%x\n", sizeof(struct free_area) * MAX_ORDER);
        debug_pf("====&free_lists[%d]: 0x%x  free_lists[%d].head: 0x%x  free_lists[%d].nr_free: 0x%x\n",
                 i, &free_lists_tmp[i],
                 i, &free_lists_tmp[i].head,
                 i, &free_lists_tmp[i].nr_free);

        // struct free_area **area = (struct free_area **)free_list_addr;
        // area[i] = &free_lists[i];
        // 问题似乎在这一句？出现了nr_free 被覆写的情况
        // 将free_lists的地址再次赋给area，但是我复现不出来了！！
        //debug_pf("area[%d]: 0x%x   free_lists[%d]'s addr: 0x%x\n", i, area[i], i, &free_lists[i]);
        // debug_pf("free_list[%d].head: 0x%x\n", i, free_lists[i].head);
    }
    
    // for (int i = MAX_ORDER - 1; i >= 0; i--) {
    //     panic_on(free_lists[i].nr_free != 0, "nr_free error");
    //     debug_pf("free_lists[%d].nr_free: 0x%x(%d)  ",
    //              i, free_lists[i].nr_free, free_lists[i].nr_free);
    //     debug_pf("now free_lists addr: 0x%x\n", &free_lists[i]);
    // }

    unsigned long free_lists_size = MAX_ORDER * sizeof(struct free_area);
    unsigned long free_lists_pages = (free_lists_size + PAGESIZE - 1) / PAGESIZE;
    debug_pf("free_lists_size: 0x%x  free_lists_pages: 0x%x(%d)\n", 
             free_lists_size, free_lists_pages, free_lists_pages);
    *start_pfn += free_lists_pages;
    debug_pf("start_pfn: 0x%x  end_pfn: 0x%x(%d)\n", *start_pfn, *end_pfn, *end_pfn);

    uintptr_t start_used = ROUNDUP(free_list_addr + MAX_ORDER * sizeof(struct free_area), PAGESIZE);
    debug_pf("start_used: 0x%x  start_pfn: 0x%x end_pfn: 0x%x\n", 
             start_used, *start_pfn, *end_pfn);

    debug_pf("==============\n");

    return free_lists_tmp;
}

// 初始化页元数据
void init_pages() {
    unsigned long start_pfn = 0, end_pfn = (HEAP_SIZE / PAGESIZE) - 1;

    size_t pagedata_size = init_page_meta_data(&start_pfn, &end_pfn);

    // 接着存储空闲链表

    struct free_area *free_list_addr = (struct free_area *)((uintptr_t)heap.start + pagedata_size);
    free_lists = (struct free_area *)free_list_addr;
    debug_pf("free_list_addr: 0x%x\n", free_list_addr);

    struct free_area *free_list = init_free_lists(&start_pfn, &end_pfn, free_list_addr);

    init_free_block(free_list, start_pfn, end_pfn);
}


