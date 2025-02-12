#include "list.h"
#include <stdint.h>
#include <buddy.h>
#include <common.h>

//struct free_area free_lists[MAX_ORDER];
struct free_area *free_lists = NULL;
uintptr_t start_used = 0;

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


// 改进，free_lists[order].head 作哨兵结点，不存储数据
//static void init_free_block(struct free_area *free_lists,
static void init_free_block(unsigned long start_pfn, unsigned long end_pfn) {
    if (start_pfn > end_pfn) panic("Invalid PFN range");
    unsigned long remaining_pages = end_pfn - start_pfn + 1;

    for (int order = MAX_ORDER - 1; order >= 0; order--) {
        unsigned long block_size = 1UL << order;

        // 分配块的头页PFN（从后向前）
        while (remaining_pages >= block_size) {
            unsigned long head_pfn = start_pfn + remaining_pages - block_size;
            struct page *head_page = pfn_to_page(head_pfn);
            unsigned long pfn = page_to_pfn(head_page);

            debug_pf("head_pfn: 0x%x  head_page: 0x%x\n",
                     head_pfn, head_page);

            head_page->used = false;
            head_page->order = order;
            head_page->compound_head = head_page;  // 头页指向自身
            
            // debug_pf("head_page->compound_head: 0x%x\n", head_page->compound_head);

            // 问题应该在这里，free_lists->head_page->buddy_list


            // 初始化尾页元数据
            for (unsigned long pfn = head_pfn + 1; pfn < head_pfn + block_size; pfn++) {
                struct page *tail_page = pfn_to_page(pfn);
                tail_page->used = false;
                tail_page->order = -1;             // 标记为尾页
                tail_page->compound_head = head_page; // 尾页指向头页
                list_add_tail(&tail_page->buddy_list, &head_page->buddy_list);
                //list_add(&tail_page->buddy_list, &head_page->buddy_list);
            }

            // 将头页的buddy_list加入free_lists[order].head
            
            // 这里搞晕了，一直不明白为什么head->next一直都不变
            // 其实用的list_add_tail，插到尾部，肯定头不变啊
            // 不过再想想，因为我是从后向前分配页的，
            // 加上自己想低阶/各阶首个空闲块地址是从小到大
            // 应该用list_add

            // debug_pf("now order is :%d  head_page: 0x%x\n",
            //          order, head_page);
            // debug_pf("& bellow:buddy_list is: 0x%x\n", &head_page->buddy_list);
            // debug_pf("add: compound_head: 0x%x  order: 0x%x  is_slab: 0x%x\n",
            //          &head_page->compound_head, &head_page->order, &head_page->is_slab);
            // debug_pf("add: used: 0x%x\n", &head_page->used);

            // debug_pf("* bellow:buddy_list is: 0x%x\n", head_page->buddy_list);
            // debug_pf("add: compound_head: 0x%x  order: 0x%x  is_slab: 0x%x\n",
            //          head_page->compound_head, head_page->order, head_page->is_slab);
            // debug_pf("add: used: 0x%x\n", head_page->used);

            // debug_pf("and now free_lists[%d].head is ; 0x%x\n",
            //          order, free_lists[order].head);
            // debug_pf("and now free_lists[%d].head's addr is ; 0x%x\n",
            //          order, &free_lists[order].head);

            if (free_lists[order].nr_free == 0) {
                debug_pf("free_lists[%d].head: next=0x%x prev=0x%x  pfn=%d\n", 
                         order, free_lists[order].head.next, 
                        free_lists[order].head.prev, pfn);
                panic_on(free_lists[order].head.next != free_lists[order].head.prev,
                         "error!!!!!! should same!!");
            }

            // 还真的是这个问题！！！是自己指针用的不到位。。。。
            //list_add(&head_page->buddy_list, (struct list_head *)(&free_lists[order].head));
            //这里如果直接用head_page->buddy_list的话，连的是这个头页的写一个页，注意指针。
            //list_add((struct list_head *)head_page, (struct list_head *)(free_lists[order].head));
            //list_add(&head_page->buddy_list, (struct list_head *)(&free_lists[order].head));
            list_add(&head_page->buddy_list, &free_lists[order].head);
            free_lists[order].nr_free++;

            remaining_pages -= block_size;

            debug_pf("free_lists[%d].nr_free: %d  free_list[%d].head: 0x%x\n",
                     order, free_lists[order].nr_free,
                     order, free_lists[order].head);
            debug_pf("free_lists[%d].head: next=0x%x prev=0x%x  pfn=%d\n", 
                     order, free_lists[order].head.next, 
                    free_lists[order].head.prev, pfn);
        
//             if (order != 0)  {
//                 if (free_lists[order].head->next == NULL) goto test;
//                 debug_pf("now order is %d\n", order);
//                 debug_pf("free_lists[order]'s addr: 0x%x  0's addr: 0x%x\n",
//                          free_lists[order].head->next, free_lists[0].head->next);
//                 debug_pf("order's head addr: 0x%x  0's addr: 0x%x\n",
//                          &free_lists[order].head, &free_lists[0].head);
//                 panic_on(((struct free_area *)(0x3fa0a0))->head->next == 
//                     ((struct free_area *)(0x3fa000))->head->next, 
//                     "different order next should not be same!!\n");
//                 //panic_on(free_lists[order].head->next == free_lists[0].head->next,
//             }
// test:
// 
            debug_pf("=============\n\n\n");
            // debug_pf("now order: %d  start_pfn: 0x%x  end_pfn: 0x%x(%d)  "
            //          "remaining_pages: 0x%x  block_size: 0x%x(%d)\n\n",
            //          order, start_pfn, end_pfn, end_pfn,
            //          remaining_pages, block_size, block_size);
        }
    }

    // for (int i = MAX_ORDER - 1; i >= 0; i--) {
    //     debug_pf("free_lists[%d].nr_free: 0x%x(%d)\n",
    //              i, free_lists[i].nr_free, free_lists[i].nr_free);
    // }
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

static void init_free_lists(unsigned long *start_pfn, 
                     unsigned long *end_pfn) {
    for (int i = 0; i < MAX_ORDER; i++) {
        debug_pf("free_lists[%d].head: 0x%x  &free_lists[%d].head: 0x%x\n",
                 i, free_lists[i].head, i, &free_lists[i].head);
        INIT_LIST_HEAD((struct list_head *)&free_lists[i].head);
        free_lists[i].nr_free = 0;

        // 这里：
        // ====sizoeof(free_lists): 0xb0
        // ====&free_lists[5]: 0x3fa050  &free_lists[5].head: 0x3fa050  &free_lists[0x5].nr_free: 0x3fa058
        // .....free_lists[5]: 0x0  free_lists[0].head: 0x5  free_lists[0x0].nr_free: 0x5
        // free_lists[6].head: 0x0  &free_lists[6].head: 0x3fa060

        debug_pf("now nr_free is: %d  and order is: %d\n",
                 free_lists[i].nr_free, i);
        debug_pf("====sizoeof(free_lists) * MAX_ORDER: 0x%x, sizoeof(free_lists): 0x%x\n",
                 sizeof(struct free_area) * MAX_ORDER,
                 sizeof(struct free_area));
        //debug_pf("====&free_lists[%d]: 0x%x  &free_lists[%d].head: 0x%x  &free_lists[%d].nr_free: 0x%x\n",
        //debug_pf(".....free_lists[%d]: 0x%x  free_lists[%d].head: 0x%x  free_lists[%d].nr_free: %d\n",
        // 上面那样写在一起就输出错的，应该是自己写的printf的bug，分开来写成下面这样就对的？
        debug_pf("====&free_lists[%d]: 0x%x\n",
                 i, &free_lists[i]);
        debug_pf("====&free_lists[%d].head: 0x%x\n",
                 i, &free_lists[i].head);
        debug_pf("====&free_lists[%d].nr_free: 0x%x\n",
                 i, &free_lists[i].nr_free);

        //debug_pf(".....free_lists[%d]: 0x%x  free_lists[%d].head: 0x%x  free_lists[%d].nr_free: %d\n",
        debug_pf(".....free_lists[%d]: 0x%x\n",
                 i, free_lists[i]);
        debug_pf(".....free_lists[%d].head: 0x%x\n",
                 i, free_lists[i].head);
        debug_pf(".....free_lists[%d].nr_free: %d\n",
                 i, free_lists[i].nr_free);

        debug_pf("free_list[%d].nr_free is %d\n", i, free_lists[i].nr_free);
    }
    
    unsigned long free_lists_size = MAX_ORDER * sizeof(struct free_area);
    unsigned long free_lists_pages = (free_lists_size + PAGESIZE - 1) / PAGESIZE;
    debug_pf("free_lists_size: 0x%x  free_lists_pages: 0x%x(%d)\n", 
             free_lists_size, free_lists_pages, free_lists_pages);
    *start_pfn += free_lists_pages;
    debug_pf("start_pfn: 0x%x  end_pfn: 0x%x(%d)\n", *start_pfn, *end_pfn, *end_pfn);

    debug_pf("==============\n");
}

// 初始化页元数据
void init_pages() {
    unsigned long start_pfn = 0, end_pfn = (HEAP_SIZE / PAGESIZE) - 1;

    size_t pagedata_size = init_page_meta_data(&start_pfn, &end_pfn);

    //struct free_area *free_list_addr = (struct free_area *)((uintptr_t)heap.start + pagedata_size);
    uintptr_t free_list_addr = ((uintptr_t)heap.start + pagedata_size);
    debug_pf("free_list_addr: 0x%x\n", free_list_addr);
    free_lists = (struct free_area *)free_list_addr;
    debug_pf("free_lists: 0x%x\n", free_lists);

    init_free_lists(&start_pfn, &end_pfn);
    start_used = ROUNDUP(free_list_addr + MAX_ORDER * sizeof(struct free_area), PAGESIZE);
    debug_pf("start_used: 0x%x  start_pfn: 0x%x end_pfn: 0x%x\n", 
             start_used, start_pfn, end_pfn);
    debug_pf("==============\n");

    //init_free_block(free_list, start_pfn, end_pfn);
    init_free_block(start_pfn, end_pfn);
}


